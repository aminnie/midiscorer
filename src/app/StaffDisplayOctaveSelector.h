#pragma once

#include <JuceHeader.h>

class StaffDisplayOctaveSelector final : public juce::ComboBox
{
public:
    static constexpr juce_wchar upArrowCharacter = 0x2191;
    static constexpr juce_wchar downArrowCharacter = 0x2193;
    static constexpr float arrowFontHeight = 20.0f;

    StaffDisplayOctaveSelector()
    {
        setLookAndFeel(&lookAndFeel);
        addItem("-", 1);
        addItem(juce::String::charToString(upArrowCharacter), 2);
        addItem(juce::String::charToString(downArrowCharacter), 3);
        setSelectedId(1, juce::dontSendNotification);
    }

    ~StaffDisplayOctaveSelector() override
    {
        setLookAndFeel(nullptr);
    }

    static bool isArrowText(const juce::String& text)
    {
        return text.length() == 1
            && (text[0] == upArrowCharacter || text[0] == downArrowCharacter);
    }

    static juce::Font arrowFont()
    {
        return juce::Font(juce::FontOptions(arrowFontHeight, juce::Font::bold));
    }

private:
    struct LookAndFeel final : public juce::LookAndFeel_V4
    {
        juce::Font getComboBoxFont(juce::ComboBox& box) override
        {
            if (isArrowText(box.getText()))
                return arrowFont();

            return juce::LookAndFeel_V4::getComboBoxFont(box);
        }

        void drawPopupMenuItem(juce::Graphics& g,
                               const juce::Rectangle<int>& area,
                               bool isSeparator,
                               bool isActive,
                               bool isHighlighted,
                               bool isTicked,
                               bool hasSubMenu,
                               const juce::String& text,
                               const juce::String& shortcutKeyText,
                               const juce::Drawable* icon,
                               const juce::Colour* textColour) override
        {
            if (! isSeparator && isArrowText(text))
            {
                if (isHighlighted && isActive)
                    g.fillAll(findColour(juce::PopupMenu::highlightedBackgroundColourId));

                auto colour = textColour != nullptr ? *textColour
                                                    : findColour(isHighlighted ? juce::PopupMenu::highlightedTextColourId
                                                                               : juce::PopupMenu::textColourId);
                if (! isActive)
                    colour = colour.withMultipliedAlpha(0.3f);

                g.setColour(colour);
                g.setFont(arrowFont());
                g.drawFittedText(text, area.reduced(4, 0), juce::Justification::centred, 1);
                return;
            }

            juce::LookAndFeel_V4::drawPopupMenuItem(g,
                                                    area,
                                                    isSeparator,
                                                    isActive,
                                                    isHighlighted,
                                                    isTicked,
                                                    hasSubMenu,
                                                    text,
                                                    shortcutKeyText,
                                                    icon,
                                                    textColour);
        }
    };

    LookAndFeel lookAndFeel;
};
