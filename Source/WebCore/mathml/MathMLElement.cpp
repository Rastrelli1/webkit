/*
 * Copyright (C) 2009 Alex Milowski (alex@milowski.com). All rights reserved.
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 François Sausset (sausset@gmail.com). All rights reserved.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MathMLElement.h"

#if ENABLE(MATHML)

#include "EventHandler.h"
#include "FrameLoader.h"
#include "HTMLAnchorElement.h"
#include "HTMLParserIdioms.h"
#include "MathMLNames.h"
#include "MouseEvent.h"
#include "RenderTableCell.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MathMLElement);

using namespace MathMLNames;

MathMLElement::MathMLElement(const QualifiedName& tagName, Document& document)
    : StyledElement(tagName, document, CreateMathMLElement)
{
}

Ref<MathMLElement> MathMLElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLElement(tagName, document));
}

unsigned MathMLElement::colSpan() const
{
    if (!hasTagName(mtdTag))
        return 1u;
    auto& colSpanValue = attributeWithoutSynchronization(columnspanAttr);
    return std::max(1u, limitToOnlyHTMLNonNegative(colSpanValue, 1u));
}

unsigned MathMLElement::rowSpan() const
{
    if (!hasTagName(mtdTag))
        return 1u;
    auto& rowSpanValue = attributeWithoutSynchronization(rowspanAttr);
    static const unsigned maxRowspan = 8190; // This constant comes from HTMLTableCellElement.
    return std::max(1u, std::min(limitToOnlyHTMLNonNegative(rowSpanValue, 1u), maxRowspan));
}

void MathMLElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == hrefAttr) {
        bool wasLink = isLink();
        setIsLink(!value.isNull() && !shouldProhibitLinks(this));
        if (wasLink != isLink())
            invalidateStyleForSubtree();
    } else if (name == rowspanAttr) {
        if (is<RenderTableCell>(renderer()) && hasTagName(mtdTag))
            downcast<RenderTableCell>(*renderer()).colSpanOrRowSpanChanged();
    } else if (name == columnspanAttr) {
        if (is<RenderTableCell>(renderer()) && hasTagName(mtdTag))
            downcast<RenderTableCell>(renderer())->colSpanOrRowSpanChanged();
    } else
        StyledElement::parseAttribute(name, value);
}

bool MathMLElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == backgroundAttr || name == colorAttr || name == dirAttr || name == fontfamilyAttr || name == fontsizeAttr || name == fontstyleAttr || name == fontweightAttr || name == mathbackgroundAttr || name == mathcolorAttr || name == mathsizeAttr)
        return true;
    return StyledElement::isPresentationAttribute(name);
}

static String convertMathSizeIfNeeded(const AtomString& value)
{
    if (value == "small")
        return "0.75em";
    if (value == "normal")
        return "1em";
    if (value == "big")
        return "1.5em";

    // FIXME: mathsize accepts any MathML length, including named spaces (see parseMathMLLength).
    // FIXME: Might be better to use double than float.
    // FIXME: Might be better to use "shortest" numeric formatting instead of fixed width.
    bool ok = false;
    float unitlessValue = value.toFloat(&ok);
    if (!ok)
        return value;
    return makeString(FormattedNumber::fixedWidth(unitlessValue * 100, 3), '%');
}

void MathMLElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomString& value, MutableStyleProperties& style)
{
    if (name == mathbackgroundAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyBackgroundColor, value);
    else if (name == mathsizeAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontSize, convertMathSizeIfNeeded(value));
    else if (name == mathcolorAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyColor, value);
    // FIXME: The following are deprecated attributes that should lose if there is a conflict with a non-deprecated attribute.
    else if (name == fontsizeAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontSize, value);
    else if (name == backgroundAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyBackgroundColor, value);
    else if (name == colorAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyColor, value);
    else if (name == fontstyleAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontStyle, value);
    else if (name == fontweightAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontWeight, value);
    else if (name == fontfamilyAttr)
        addPropertyToPresentationAttributeStyle(style, CSSPropertyFontFamily, value);
    else if (name == dirAttr) {
        if (hasTagName(mathTag) || hasTagName(mrowTag) || hasTagName(mstyleTag) || isMathMLToken())
            addPropertyToPresentationAttributeStyle(style, CSSPropertyDirection, value);
    }  else {
        ASSERT(!isPresentationAttribute(name));
        StyledElement::collectStyleForPresentationAttribute(name, value, style);
    }
}

bool MathMLElement::childShouldCreateRenderer(const Node& child) const
{
    // In general, only MathML children are allowed. Text nodes are only visible in token MathML elements.
    return is<MathMLElement>(child);
}

bool MathMLElement::willRespondToMouseClickEvents()
{
    return isLink() || StyledElement::willRespondToMouseClickEvents();
}

void MathMLElement::defaultEventHandler(Event& event)
{
    if (isLink()) {
        if (focused() && isEnterKeyKeydownEvent(event)) {
            event.setDefaultHandled();
            dispatchSimulatedClick(&event);
            return;
        }
        if (MouseEvent::canTriggerActivationBehavior(event)) {
            auto& href = attributeWithoutSynchronization(hrefAttr);
            const auto& url = stripLeadingAndTrailingHTMLSpaces(href);
            event.setDefaultHandled();
            if (auto* frame = document().frame())
                frame->loader().urlSelected(document().completeURL(url), "_self", &event, LockHistory::No, LockBackForwardList::No, MaybeSendReferrer, document().shouldOpenExternalURLsPolicyToPropagate());
            return;
        }
    }

    StyledElement::defaultEventHandler(event);
}

bool MathMLElement::canStartSelection() const
{
    if (!isLink())
        return StyledElement::canStartSelection();

    return hasEditableStyle();
}

bool MathMLElement::isKeyboardFocusable(KeyboardEvent* event) const
{
    if (isFocusable() && StyledElement::supportsFocus())
        return StyledElement::isKeyboardFocusable(event);

    if (isLink())
        return document().frame()->eventHandler().tabsToLinks(event);

    return StyledElement::isKeyboardFocusable(event);
}

bool MathMLElement::isMouseFocusable() const
{
    // Links are focusable by default, but only allow links with tabindex or contenteditable to be mouse focusable.
    // https://bugs.webkit.org/show_bug.cgi?id=26856
    if (isLink())
        return StyledElement::supportsFocus();

    return StyledElement::isMouseFocusable();
}

bool MathMLElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name().localName() == hrefAttr || StyledElement::isURLAttribute(attribute);
}

bool MathMLElement::supportsFocus() const
{
    if (hasEditableStyle())
        return StyledElement::supportsFocus();
    // If not a link we should still be able to focus the element if it has tabIndex.
    return isLink() || StyledElement::supportsFocus();
}

int MathMLElement::defaultTabIndex() const
{
    // FIXME: This seems wrong.
    return 0;
}

}

#endif // ENABLE(MATHML)
