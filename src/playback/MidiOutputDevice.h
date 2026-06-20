#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>

struct MidiOutputDeviceInfo
{
    juce::String identifier;
    juce::String name;
};

class MidiOutputDevice
{
public:
    static std::vector<MidiOutputDeviceInfo> enumerateAvailableOutputs()
    {
        std::vector<MidiOutputDeviceInfo> outputs;
        const auto devices = juce::MidiOutput::getAvailableDevices();
        outputs.reserve((size_t) devices.size());
        for (const auto& device : devices)
            outputs.push_back({ device.identifier, device.name });
        return outputs;
    }

    bool openByIdentifier(const juce::String& identifier, juce::String& error)
    {
        close();
        error.clear();

        if (identifier.isEmpty())
        {
            error = "No MIDI output selected.";
            return false;
        }

        auto output = juce::MidiOutput::openDevice(identifier);
        if (output == nullptr)
        {
            error = "Failed to open MIDI output device.";
            return false;
        }

        selectedIdentifier = identifier;
        selectedName = resolveNameForIdentifier(identifier);
        device = std::move(output);
        return true;
    }

    void close()
    {
        device.reset();
        selectedIdentifier.clear();
        selectedName.clear();
    }

    bool isOpen() const { return device != nullptr; }
    juce::String getSelectedIdentifier() const { return selectedIdentifier; }
    juce::String getSelectedName() const { return selectedName; }

    void sendMessageNow(const juce::MidiMessage& message)
    {
        if (device != nullptr)
            device->sendMessageNow(message);
    }

    void sendAllNotesOff()
    {
        if (device == nullptr)
            return;

        for (int channel = 1; channel <= 16; ++channel)
            device->sendMessageNow(juce::MidiMessage::allNotesOff(channel));
    }

private:
    static juce::String resolveNameForIdentifier(const juce::String& identifier)
    {
        for (const auto& deviceInfo : juce::MidiOutput::getAvailableDevices())
        {
            if (deviceInfo.identifier == identifier)
                return deviceInfo.name;
        }
        return identifier;
    }

    std::unique_ptr<juce::MidiOutput> device;
    juce::String selectedIdentifier;
    juce::String selectedName;
};
