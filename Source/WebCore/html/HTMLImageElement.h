/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2008, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "DecodingOptions.h"
#include "FormNamedItem.h"
#include "GraphicsLayer.h"
#include "GraphicsTypes.h"
#include "HTMLElement.h"
#include "HTMLImageLoader.h"

namespace WebCore {

class EditableImageReference;
class HTMLAttachmentElement;
class HTMLFormElement;
class HTMLMapElement;

struct ImageCandidate;

class HTMLImageElement : public HTMLElement, public FormNamedItem {
    WTF_MAKE_ISO_ALLOCATED(HTMLImageElement);
    friend class HTMLFormElement;
public:
    static Ref<HTMLImageElement> create(Document&);
    static Ref<HTMLImageElement> create(const QualifiedName&, Document&, HTMLFormElement*);
    static Ref<HTMLImageElement> createForJSConstructor(Document&, Optional<unsigned> width, Optional<unsigned> height);

    virtual ~HTMLImageElement();

    WEBCORE_EXPORT unsigned width(bool ignorePendingStylesheets = false);
    WEBCORE_EXPORT unsigned height(bool ignorePendingStylesheets = false);

    WEBCORE_EXPORT int naturalWidth() const;
    WEBCORE_EXPORT int naturalHeight() const;
    const AtomString& currentSrc() const { return m_currentSrc; }

    bool supportsFocus() const override;
    bool isFocusable() const override;

    bool isServerMap() const;

    const AtomString& altText() const;

    CompositeOperator compositeOperator() const { return m_compositeOperator; }

    CachedImage* cachedImage() const { return m_imageLoader.image(); }

    void setLoadManually(bool loadManually) { m_imageLoader.setLoadManually(loadManually); }

    bool matchesUsemap(const AtomStringImpl&) const;
    HTMLMapElement* associatedMapElement() const;

    WEBCORE_EXPORT const AtomString& alt() const;

    WEBCORE_EXPORT void setHeight(unsigned);

    URL src() const;
    void setSrc(const String&);

    WEBCORE_EXPORT void setCrossOrigin(const AtomString&);
    WEBCORE_EXPORT String crossOrigin() const;

    WEBCORE_EXPORT void setWidth(unsigned);

    WEBCORE_EXPORT int x() const;
    WEBCORE_EXPORT int y() const;

    WEBCORE_EXPORT bool complete() const;

    DecodingMode decodingMode() const;
    
    WEBCORE_EXPORT void decode(Ref<DeferredPromise>&&);

#if PLATFORM(IOS_FAMILY)
    bool willRespondToMouseClickEvents() override;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    void setAttachmentElement(Ref<HTMLAttachmentElement>&&);
    RefPtr<HTMLAttachmentElement> attachmentElement() const;
    const String& attachmentIdentifier() const;
#endif

    bool hasPendingActivity() const { return m_imageLoader.hasPendingActivity(); }

    bool canContainRangeEndPoint() const override { return false; }

    const AtomString& imageSourceURL() const override;

    bool hasShadowControls() const { return m_experimentalImageMenuEnabled; }
    
    HTMLPictureElement* pictureElement() const;
    void setPictureElement(HTMLPictureElement*);

#if USE(SYSTEM_PREVIEW)
    WEBCORE_EXPORT bool isSystemPreviewImage() const;
#endif

    WEBCORE_EXPORT GraphicsLayer::EmbeddedViewID editableImageViewID() const;
    WEBCORE_EXPORT bool hasEditableImageAttribute() const;

    void defaultEventHandler(Event&) final;

protected:
    HTMLImageElement(const QualifiedName&, Document&, HTMLFormElement* = 0);

    void didMoveToNewDocument(Document& oldDocument, Document& newDocument) override;

private:
    void parseAttribute(const QualifiedName&, const AtomString&) override;
    bool isPresentationAttribute(const QualifiedName&) const override;
    void collectStyleForPresentationAttribute(const QualifiedName&, const AtomString&, MutableStyleProperties&) override;

    void didAttachRenderers() override;
    RenderPtr<RenderElement> createElementRenderer(RenderStyle&&, const RenderTreePosition&) override;
    void setBestFitURLAndDPRFromImageCandidate(const ImageCandidate&);

    bool canStartSelection() const override;

    bool isURLAttribute(const Attribute&) const override;
    bool attributeContainsURL(const Attribute&) const override;
    String completeURLsInAttributeValue(const URL& base, const Attribute&) const override;

    bool draggable() const override;

    void addSubresourceAttributeURLs(ListHashSet<URL>&) const override;

    InsertedIntoAncestorResult insertedIntoAncestor(InsertionType, ContainerNode&) override;
    void didFinishInsertingNode() override;
    void removedFromAncestor(RemovalType, ContainerNode&) override;

    bool isFormAssociatedElement() const final { return false; }
    FormNamedItem* asFormNamedItem() final { return this; }
    HTMLImageElement& asHTMLElement() final { return *this; }
    const HTMLImageElement& asHTMLElement() const final { return *this; }

    void selectImageSource();

    ImageCandidate bestFitSourceFromPictureElement();

    void updateEditableImage();

    void copyNonAttributePropertiesFromElement(const Element&) final;

#if ENABLE(SERVICE_CONTROLS)
    void updateImageControls();
    void tryCreateImageControls();
    void destroyImageControls();
    bool hasImageControls() const;
    bool childShouldCreateRenderer(const Node&) const override;
#endif

    HTMLImageLoader m_imageLoader;
    WeakPtr<HTMLFormElement> m_form;
    WeakPtr<HTMLFormElement> m_formSetByParser;

    CompositeOperator m_compositeOperator;
    AtomString m_bestFitImageURL;
    AtomString m_currentSrc;
    AtomString m_parsedUsemap;
    float m_imageDevicePixelRatio;
    bool m_experimentalImageMenuEnabled;
    bool m_hadNameBeforeAttributeChanged { false }; // FIXME: We only need this because parseAttribute() can't see the old value.

    RefPtr<EditableImageReference> m_editableImage;
    WeakPtr<HTMLPictureElement> m_pictureElement;

#if ENABLE(ATTACHMENT_ELEMENT)
    String m_pendingClonedAttachmentID;
#endif

    friend class HTMLPictureElement;
};

} //namespace
