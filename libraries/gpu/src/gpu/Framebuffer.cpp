//
//  Framebuffer.cpp
//  libraries/gpu/src/gpu
//
//  Created by Sam Gateau on 4/12/2015.
//  Copyright 2014 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include "Framebuffer.h"
#include <math.h>
#include <QDebug>

using namespace gpu;

Framebuffer::Framebuffer()
{
}

Framebuffer::~Framebuffer()
{
}

Framebuffer* Framebuffer::create() {
    auto framebuffer = new Framebuffer();
    framebuffer->_renderBuffers.resize(MAX_NUM_RENDER_BUFFERS);
    framebuffer->_renderBuffersSubresource.resize(MAX_NUM_RENDER_BUFFERS, 0);
    return framebuffer;
}

Framebuffer* Framebuffer::create( const Format& colorBufferFormat, const Format& depthStencilBufferFormat, uint16 width, uint16 height, uint16 numSamples) {
    auto framebuffer = Framebuffer::create();

    auto colorTexture = TexturePointer(Texture::create2D(colorBufferFormat, width, height));
    auto depthTexture = TexturePointer(Texture::create2D(depthStencilBufferFormat, width, height));

    framebuffer->setRenderBuffer(0, colorTexture);
    framebuffer->setDepthStencilBuffer(depthTexture);

    return framebuffer;
}


bool Framebuffer::isSwapchain() const {
    return _swapchain != 0;
}

uint32 Framebuffer::getFrameCount() const {
    if (_swapchain) {
        return _swapchain->getFrameCount();
    } else {
        return _frameCount;
    }
}

bool Framebuffer::isEmpty() const {
    return (_buffersMask == 0);
}

bool Framebuffer::validateTargetCompatibility(const Texture& texture, uint32 subresource) const {
    if (texture.getType() == Texture::TEX_1D) {
        return false;
    }

    if (isEmpty()) {
        return true;
    } else {
        if ((texture.getWidth() == getWidth()) && 
            (texture.getHeight() == getHeight()) && 
            (texture.getNumSamples() == getNumSamples())) {
            return true;
        } else {
            return false;
        }
    }
}

void Framebuffer::updateSize(const TexturePointer& texture) {
    if (!isEmpty()) {
        return;
    }

    if (texture) {
        _width = texture->getWidth();
        _height = texture->getHeight();
        _numSamples = texture->getNumSamples();
    } else {
        _width = _height = _numSamples = 0;
    }
}

void Framebuffer::resize(uint16 width, uint16 height, uint16 numSamples) {
    if (width && height && numSamples && !isEmpty() && !isSwapchain()) {
        if ((width != _width) || (height != _height) || (numSamples != _numSamples)) {
            for (uint32 i = 0; i < _renderBuffers.size(); ++i) {
                if (_renderBuffers[i]) {
                    _renderBuffers[i]->resize2D(width, height, numSamples);
                    _numSamples = _renderBuffers[i]->getNumSamples();
                }
            }

            if (_depthStencilBuffer) {
                _depthStencilBuffer->resize2D(width, height, numSamples);
                _numSamples = _depthStencilBuffer->getNumSamples();
            }

            _width = width;
            _height = height;
         //   _numSamples = numSamples;
        }
    }
}

uint16 Framebuffer::getWidth() const {
    if (isSwapchain()) {
        return getSwapchain()->getWidth();
    } else {
        return _width;
    }
}

uint16 Framebuffer::getHeight() const {
    if (isSwapchain()) {
        return getSwapchain()->getHeight();
    } else {
        return _height;
    }
}

uint16 Framebuffer::getNumSamples() const {
    if (isSwapchain()) {
        return getSwapchain()->getNumSamples();
    } else {
        return _numSamples;
    }
}

// Render buffers
int Framebuffer::setRenderBuffer(uint32 slot, const TexturePointer& texture, uint32 subresource) {
    if (isSwapchain()) {
        return -1;
    }

    // Check for the slot
    if (slot >= getMaxNumRenderBuffers()) {
        return -1;
    }

    // Check for the compatibility of size
    if (texture) {
        if (!validateTargetCompatibility(*texture, subresource)) {
            return -1;
        }
    }

    // everything works, assign
    // dereference the previously used buffer if exists
    if (_renderBuffers[slot]) {
        _renderBuffers[slot].reset();
        _renderBuffersSubresource[slot] = 0;
    }

    updateSize(texture);

    // assign the new one
    _renderBuffers[slot] = texture;

    // Assign the subresource
    _renderBuffersSubresource[slot] = subresource;

    // update the mask
    int mask = (1<<slot);
    _buffersMask = (_buffersMask & ~(mask));
    if (texture) {
        _buffersMask |= mask;
    }

    return slot;
}

void Framebuffer::removeRenderBuffers() {
    if (isSwapchain()) {
        return;
    }

    _buffersMask = _buffersMask & BUFFER_DEPTHSTENCIL;

    for (auto renderBuffer : _renderBuffers) {
        renderBuffer.reset();
    }

    updateSize(TexturePointer(nullptr));
}


uint32 Framebuffer::getNumRenderBuffers() const {
    uint32 nb = 0;
    for (auto i : _renderBuffers) {
        nb += (!i);
    }
    return nb;
}

TexturePointer Framebuffer::getRenderBuffer(uint32 slot) const {
    if (!isSwapchain() && (slot < getMaxNumRenderBuffers())) {
        return _renderBuffers[slot];
    } else {
        return TexturePointer();
    }

}

uint32 Framebuffer::getRenderBufferSubresource(uint32 slot) const {
    if (!isSwapchain() && (slot < getMaxNumRenderBuffers())) {
        return _renderBuffersSubresource[slot];
    } else {
        return 0;
    }
}

bool Framebuffer::setDepthStencilBuffer(const TexturePointer& texture, uint32 subresource) {
    if (isSwapchain()) {
        return false;
    }

    // Check for the compatibility of size
    if (texture) {
        if (!validateTargetCompatibility(*texture)) {
            return false;
        }
    }

    // Checks ok, assign
    _depthStencilBuffer.reset();
    _depthStencilBufferSubresource = 0;

    updateSize(texture);

    // assign the new one
    _depthStencilBuffer = texture;
    _depthStencilBufferSubresource = subresource;

    _buffersMask = ( _buffersMask & ~BUFFER_DEPTHSTENCIL);
    if (texture) {
        _buffersMask |= BUFFER_DEPTHSTENCIL;
    }

    return true;
}

TexturePointer Framebuffer::getDepthStencilBuffer() const {
    if (isSwapchain()) {
        return TexturePointer();
    } else {
        return _depthStencilBuffer;
    }
}

uint32 Framebuffer::getDepthStencilBufferSubresource() const {
    if (isSwapchain()) {
        return 0;
    } else {
        return _depthStencilBufferSubresource;
    }
}
