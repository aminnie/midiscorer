#pragma once

#include <JuceHeader.h>
#include <optional>
#include <vector>

struct InstrumentVoice
{
    juce::String name;
    int bankMsb = 0;
    int bankLsb = 0;
    int program = 0;
};

struct InstrumentCategory
{
    juce::String name;
    std::vector<InstrumentVoice> voices;
};

struct InstrumentModule
{
    juce::String id;
    juce::String displayName;
    juce::String fileName;
};

class InstrumentCatalog
{
public:
    InstrumentCatalog()
    {
        catalogRoot = resolveCatalogRoot();
        modules = loadModules();
        juce::String ignoredError;
        (void) loadModuleByIndex(0, ignoredError);
    }

    const std::vector<InstrumentModule>& getModules() const { return modules; }
    const std::vector<InstrumentCategory>& getCategories() const { return categories; }
    const juce::String& getVendorName() const { return vendorName; }
    int getSelectedModuleIndex() const { return selectedModuleIndex; }

    juce::String getSelectedModuleName() const
    {
        if (selectedModuleIndex < 0 || selectedModuleIndex >= static_cast<int>(modules.size()))
            return {};
        return modules[(size_t) selectedModuleIndex].displayName;
    }

    bool loadModuleByIndex(int moduleIndex, juce::String& error)
    {
        categories.clear();
        vendorName.clear();
        error.clear();

        if (modules.empty())
        {
            error = "No sound modules are available.";
            return false;
        }

        const int clamped = juce::jlimit(0, static_cast<int>(modules.size()) - 1, moduleIndex);
        const auto& module = modules[(size_t) clamped];
        const auto jsonFile = catalogRoot.getChildFile(module.fileName);
        if (!jsonFile.existsAsFile())
        {
            error = "Missing sound catalog: " + jsonFile.getFullPathName();
            return false;
        }

        juce::var parsed;
        const auto parseResult = juce::JSON::parse(jsonFile.loadFileAsString(), parsed);
        if (parseResult.failed() || !parsed.isObject())
        {
            error = "Could not parse " + module.fileName;
            return false;
        }

        auto* root = parsed.getDynamicObject();
        if (root == nullptr)
        {
            error = "Catalog root is invalid in " + module.fileName;
            return false;
        }

        vendorName = root->getProperty("Vendor").toString().trim();
        auto* instruments = root->getProperty("Instruments").getArray();
        if (instruments == nullptr)
        {
            error = "Catalog has no Instruments array.";
            return false;
        }

        for (const auto& categoryVar : *instruments)
        {
            auto* categoryArray = categoryVar.getArray();
            if (categoryArray == nullptr || categoryArray->isEmpty())
                continue;

            InstrumentCategory category;
            category.name = categoryArray->getFirst().toString();
            for (int i = 1; i < categoryArray->size(); ++i)
            {
                auto* voiceArray = categoryArray->getReference(i).getArray();
                if (voiceArray == nullptr || voiceArray->size() < 5)
                    continue;

                InstrumentVoice voice;
                voice.program = juce::jlimit(0, 127, static_cast<int>(voiceArray->getReference(1)));
                voice.bankMsb = juce::jlimit(0, 127, static_cast<int>(voiceArray->getReference(2)));
                voice.bankLsb = juce::jlimit(0, 127, static_cast<int>(voiceArray->getReference(3)));
                voice.name = voiceArray->getReference(4).toString().trim();
                if (voice.name.isNotEmpty())
                    category.voices.push_back(std::move(voice));
            }

            if (!category.voices.empty())
                categories.push_back(std::move(category));
        }

        selectedModuleIndex = clamped;
        return !categories.empty();
    }

    std::optional<InstrumentVoice> findVoice(const juce::String& categoryName, int voiceIndex) const
    {
        for (const auto& category : categories)
        {
            if (!category.name.equalsIgnoreCase(categoryName))
                continue;

            if (voiceIndex < 0 || voiceIndex >= static_cast<int>(category.voices.size()))
                return std::nullopt;
            return category.voices[(size_t) voiceIndex];
        }
        return std::nullopt;
    }

    std::optional<InstrumentVoice> findVoiceByProgram(int bankMsb, int bankLsb, int program) const
    {
        for (const auto& category : categories)
        {
            for (const auto& voice : category.voices)
            {
                if (voice.bankMsb == bankMsb && voice.bankLsb == bankLsb && voice.program == program)
                    return voice;
            }
        }
        return std::nullopt;
    }

private:
    static juce::File resolveCatalogRoot()
    {
        const auto cwd = juce::File::getCurrentWorkingDirectory();
        const auto cwdResources = cwd.getChildFile("resources").getChildFile("instruments");
        if (cwdResources.isDirectory())
            return cwdResources;

        auto base = juce::File::getSpecialLocation(juce::File::currentApplicationFile).getParentDirectory();
        for (int i = 0; i < 6 && base.exists(); ++i)
        {
            const auto candidate = base.getChildFile("resources").getChildFile("instruments");
            if (candidate.isDirectory())
                return candidate;
            base = base.getParentDirectory();
        }

        return cwdResources;
    }

    juce::File getModuleConfigPath() const
    {
        auto configRoot = catalogRoot.getParentDirectory();
        if (!configRoot.exists())
            configRoot = juce::File::getCurrentWorkingDirectory().getChildFile("resources");
        return configRoot.getChildFile("configs").getChildFile("instrument_modules.json");
    }

    std::vector<InstrumentModule> loadModules() const
    {
        std::vector<InstrumentModule> loaded;
        const auto configFile = getModuleConfigPath();
        if (configFile.existsAsFile())
        {
            juce::var parsed;
            const auto parseResult = juce::JSON::parse(configFile.loadFileAsString(), parsed);
            if (!parseResult.failed() && parsed.isObject())
            {
                if (auto* root = parsed.getDynamicObject())
                {
                    if (auto* modulesVar = root->getProperty("modules").getArray())
                    {
                        for (const auto& moduleVar : *modulesVar)
                        {
                            auto* moduleObj = moduleVar.getDynamicObject();
                            if (moduleObj == nullptr)
                                continue;

                            InstrumentModule module;
                            module.id = moduleObj->getProperty("moduleIdString").toString().trim();
                            module.displayName = moduleObj->getProperty("displayName").toString().trim();
                            module.fileName = moduleObj->getProperty("moduleFileName").toString().trim();
                            if (module.displayName.isNotEmpty() && module.fileName.isNotEmpty())
                                loaded.push_back(std::move(module));
                        }
                    }
                }
            }
        }

        if (!loaded.empty())
            return loaded;

        loaded.push_back({ "MIDI", "MidiGM", "midigm.json" });
        loaded.push_back({ "BLACKBOX", "Deebach BlackBox", "maxplus.json" });
        return loaded;
    }

    juce::File catalogRoot;
    std::vector<InstrumentModule> modules;
    std::vector<InstrumentCategory> categories;
    juce::String vendorName;
    int selectedModuleIndex = 0;
};
