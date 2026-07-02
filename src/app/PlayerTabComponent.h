#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "MainComponent.h"

class PlayerTabComponent final : public juce::Component,
                                 private juce::Timer
{
public:
    explicit PlayerTabComponent(MainComponent& scorePageRef)
        : scorePage(scorePageRef)
    {
        addAndMakeVisible(fileLabel);
        fileLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(statusLabel);
        statusLabel.setJustificationType(juce::Justification::centredLeft);

        addAndMakeVisible(outputLabel);
        outputLabel.setText("MIDI Output", juce::dontSendNotification);
        outputLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(refreshOutputsButton);
        refreshOutputsButton.setButtonText("Refresh");
        applyStartButtonStyle(refreshOutputsButton);
        refreshOutputsButton.onClick = [this] { refreshOutputDeviceList(); };

        addAndMakeVisible(startupResumeToggle);
        startupResumeToggle.setButtonText("Reopen last MIDI on startup");
        startupResumeToggle.setToggleState(scorePage.isStartupResumeEnabled(), juce::dontSendNotification);
        startupResumeToggle.setTooltip("When enabled, MidiScorer reopens the last loaded MIDI file (or most recent file) on launch.");
        startupResumeToggle.onClick = [this]
        {
            scorePage.setStartupResumeEnabled(startupResumeToggle.getToggleState());
        };

        addAndMakeVisible(workingDirectoryLabel);
        workingDirectoryLabel.setText("Working Directory", juce::dontSendNotification);
        workingDirectoryLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(workingDirectoryInput);
        workingDirectoryInput.setText(scorePage.getWorkingDirectoryPath(), juce::dontSendNotification);
        workingDirectoryInput.setTooltip("MIDI files loaded from outside this folder are copied here automatically.");
        workingDirectoryInput.onReturnKey = [this] { applyWorkingDirectoryInput(); };
        workingDirectoryInput.onFocusLost = [this] { applyWorkingDirectoryInput(); };

        addAndMakeVisible(browseWorkingDirectoryButton);
        browseWorkingDirectoryButton.setButtonText("Set Working Directory");
        applyStartButtonStyle(browseWorkingDirectoryButton);
        browseWorkingDirectoryButton.onClick = [this] { browseWorkingDirectory(); };

        addAndMakeVisible(addWorkingSubfolderButton);
        addWorkingSubfolderButton.setButtonText("Add Subfolder");
        addWorkingSubfolderButton.setTooltip("Create a new folder under the selected working directory.");
        applyStartButtonStyle(addWorkingSubfolderButton);
        addWorkingSubfolderButton.onClick = [this] { addWorkingSubfolder(); };

        addAndMakeVisible(workingFilesLabel);
        workingFilesLabel.setText("Working MIDI", juce::dontSendNotification);
        workingFilesLabel.setJustificationType(juce::Justification::centredRight);

        addAndMakeVisible(workingFilesSelector);
        workingFilesSelector.onChange = [this] { refreshWorkingFileButtonsEnabledState(); };

        addAndMakeVisible(addWorkingMidiButton);
        addWorkingMidiButton.setButtonText("Add MIDI");
        applyStartButtonStyle(addWorkingMidiButton);
        addWorkingMidiButton.onClick = [this] { addMidiToWorkingDirectory(); };

        addAndMakeVisible(renameWorkingMidiButton);
        renameWorkingMidiButton.setButtonText("Rename MIDI");
        applyStartButtonStyle(renameWorkingMidiButton);
        renameWorkingMidiButton.onClick = [this] { renameSelectedWorkingMidi(); };

        addAndMakeVisible(deleteWorkingMidiButton);
        deleteWorkingMidiButton.setButtonText("Delete MIDI");
        applyStartButtonStyle(deleteWorkingMidiButton);
        deleteWorkingMidiButton.onClick = [this] { deleteSelectedWorkingMidi(); };

        addAndMakeVisible(scoreLightModeToggle);
        scoreLightModeToggle.setButtonText("Light Score");
        scoreLightModeToggle.setToggleState(scorePage.isScoreLightMode(), juce::dontSendNotification);
        scoreLightModeToggle.setTooltip("Toggle score display between white-on-black and black-on-white.");
        scoreLightModeToggle.onClick = [this]
        {
            scorePage.setScoreLightMode(scoreLightModeToggle.getToggleState());
        };

        addAndMakeVisible(outputSelector);
        outputSelector.onChange = [this] { handleOutputSelectionChanged(); };

        addAndMakeVisible(exitButton);
        exitButton.setButtonText("Exit");
        applyStartButtonStyle(exitButton);
        exitButton.onClick = [this]()
        {
            if (exitAction)
                exitAction();
        };

        refreshOutputDeviceList();
        refreshLiveStatus();
        refreshWorkingMidiFileList();
        refreshWorkingFileButtonsEnabledState();
        startTimerHz(5);
    }

    void setExitAction(std::function<void()> action)
    {
        exitAction = std::move(action);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(12);
        auto row1 = area.removeFromTop(28);
        outputLabel.setBounds(row1.removeFromLeft(90));
        outputSelector.setBounds(row1.removeFromLeft(360).reduced(4, 0));
        refreshOutputsButton.setBounds(row1.removeFromLeft(96).reduced(4, 0));
        exitButton.setBounds(row1.removeFromRight(80).reduced(4, 0));

        area.removeFromTop(8);
        auto workingRow = area.removeFromTop(28);
        constexpr int workingLabelWidth = 116;
        constexpr int workingInputWidth = 470;
        constexpr int workingActionWidth = 188;
        workingDirectoryLabel.setBounds(workingRow.removeFromLeft(116));
        workingDirectoryInput.setBounds(workingRow.removeFromLeft(workingInputWidth).reduced(4, 0));
        browseWorkingDirectoryButton.setBounds(workingRow.removeFromLeft(workingActionWidth).reduced(4, 0));

        area.removeFromTop(6);
        auto subfolderRow = area.removeFromTop(28);
        subfolderRow.removeFromLeft(workingLabelWidth);
        subfolderRow.removeFromLeft(workingInputWidth);
        addWorkingSubfolderButton.setBounds(subfolderRow.removeFromLeft(workingActionWidth).reduced(4, 0));

        area.removeFromTop(6);
        auto filesRow = area.removeFromTop(28);
        workingFilesLabel.setBounds(filesRow.removeFromLeft(116));
        workingFilesSelector.setBounds(filesRow.removeFromLeft(470).reduced(4, 0));
        addWorkingMidiButton.setBounds(filesRow.removeFromLeft(96).reduced(4, 0));
        renameWorkingMidiButton.setBounds(filesRow.removeFromLeft(108).reduced(4, 0));
        deleteWorkingMidiButton.setBounds(filesRow.removeFromLeft(96).reduced(4, 0));

        area.removeFromTop(8);
        auto toggleRow = area.removeFromTop(28);
        startupResumeToggle.setBounds(toggleRow.removeFromLeft(260));
        scoreLightModeToggle.setBounds(toggleRow.removeFromLeft(120));

        area.removeFromTop(8);
        fileLabel.setBounds(area.removeFromTop(24));

        area.removeFromTop(8);
        statusLabel.setBounds(area.removeFromTop(24));
    }

private:
    static void applyStartButtonStyle(juce::TextButton& button)
    {
        const auto baseOff = juce::Colours::lightgrey;
        const auto baseOn = juce::Colours::lightgrey;
        const auto baseText = juce::Colours::black;
        button.setColour(juce::TextButton::buttonColourId, baseOff);
        button.setColour(juce::TextButton::buttonOnColourId, baseOn);
        button.setColour(juce::TextButton::textColourOffId, baseText);
        button.setColour(juce::TextButton::textColourOnId, baseText);
    }

    void timerCallback() override
    {
        refreshLiveStatus();
    }

    void refreshOutputDeviceList()
    {
        availableOutputs = scorePage.getAvailableMidiOutputs();
        outputSelector.clear(juce::dontSendNotification);
        outputSelector.addItem("(None)", 1);

        int itemId = 2;
        int selectedId = 1;
        const auto currentSelection = scorePage.getSelectedMidiOutputIdentifier();
        for (const auto& output : availableOutputs)
        {
            outputSelector.addItem(output.name, itemId);
            if (output.identifier == currentSelection)
                selectedId = itemId;
            ++itemId;
        }

        outputSelector.setSelectedId(selectedId, juce::dontSendNotification);
    }

    void handleOutputSelectionChanged()
    {
        const int selectedId = outputSelector.getSelectedId();
        if (selectedId <= 1)
        {
            scorePage.clearMidiOutputDevice();
            refreshLiveStatus();
            return;
        }

        const int index = selectedId - 2;
        if (index < 0 || index >= static_cast<int>(availableOutputs.size()))
            return;

        juce::String error;
        const auto selectedIdentifier = availableOutputs[(size_t) index].identifier;
        const bool selected = scorePage.selectMidiOutputDevice(selectedIdentifier, true, error);
        if (!selected)
            statusLabel.setText("MIDI output error: " + error, juce::dontSendNotification);
    }

    void refreshLiveStatus()
    {
        const auto midiName = scorePage.getLoadedMidiFileName();
        if (midiName.isNotEmpty())
        {
            juce::String text = "Loaded MIDI: " + midiName;
            if (scorePage.isLoadedMidiInWorkingDirectory())
                text << " (Working Directory)";
            else
                text << " (Outside Working Directory)";
            fileLabel.setText(text, juce::dontSendNotification);
        }
        else
        {
            fileLabel.setText("Loaded MIDI: none", juce::dontSendNotification);
        }

        const auto workingDirPath = scorePage.getWorkingDirectoryPath();
        if (workingDirectoryInput.getText() != workingDirPath)
            workingDirectoryInput.setText(workingDirPath, juce::dontSendNotification);
        refreshWorkingMidiFileList();

        const bool isPlaying = scorePage.isPlaybackRunning();
        const int currentBar = scorePage.getCurrentPlaybackBar();
        const int maxBar = scorePage.getMaximumBar();
        auto outputName = scorePage.getSelectedMidiOutputName();
        if (outputName.isEmpty())
            outputName = "(None)";

        juce::String statusText = juce::String(isPlaying ? "Playing" : "Stopped")
            + "  | Bar " + juce::String(currentBar) + "/" + juce::String(maxBar)
            + "  | Output: " + outputName;
        const auto restoreWarning = scorePage.getMidiOutputRestoreWarning();
        if (restoreWarning.isNotEmpty())
            statusText << "  | Warning: " << restoreWarning;
        statusLabel.setText(statusText, juce::dontSendNotification);

        const bool lightScore = scorePage.isScoreLightMode();
        if (scoreLightModeToggle.getToggleState() != lightScore)
            scoreLightModeToggle.setToggleState(lightScore, juce::dontSendNotification);
    }

    void applyWorkingDirectoryInput()
    {
        juce::String error;
        if (!scorePage.setWorkingDirectoryPath(workingDirectoryInput.getText(), error))
        {
            statusLabel.setText("Working directory error: " + error, juce::dontSendNotification);
            workingDirectoryInput.setText(scorePage.getWorkingDirectoryPath(), juce::dontSendNotification);
            return;
        }

        workingDirectoryInput.setText(scorePage.getWorkingDirectoryPath(), juce::dontSendNotification);
        refreshWorkingMidiFileList();
    }

    void browseWorkingDirectory()
    {
        workingDirectoryChooser = std::make_unique<juce::FileChooser>(
            "Set working directory",
            juce::File(scorePage.getWorkingDirectoryPath()),
            juce::String(),
            false,
            false,
            this);
        const auto chooserFlags = juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectDirectories;
        workingDirectoryChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
        {
            auto selected = chooser.getResult();
            if (!selected.exists())
                return;

            if (!selected.isDirectory())
                return;

            workingDirectoryInput.setText(selected.getFullPathName(), juce::dontSendNotification);
            applyWorkingDirectoryInput();
        });
    }

    static juce::String sanitizeMidiBaseName(const juce::String& input)
    {
        auto name = input.trim().removeCharacters("\\/:*?\"<>|");
        while (name.isNotEmpty() && (name.endsWithChar('.') || name.endsWithChar(' ')))
            name = name.dropLastCharacters(1);
        return name.trim();
    }

    juce::String getSelectedWorkingMidiFileName() const
    {
        const int selectedId = workingFilesSelector.getSelectedId();
        if (selectedId <= 1 || selectedId - 2 >= static_cast<int>(workingMidiFileNames.size()))
            return {};
        return workingMidiFileNames[(size_t) (selectedId - 2)];
    }

    void refreshWorkingFileButtonsEnabledState()
    {
        const bool hasSelection = getSelectedWorkingMidiFileName().isNotEmpty();
        renameWorkingMidiButton.setEnabled(hasSelection);
        deleteWorkingMidiButton.setEnabled(hasSelection);
    }

    void refreshWorkingMidiFileList()
    {
        const auto files = scorePage.getWorkingDirectoryMidiFiles();
        std::vector<juce::String> fileNames;
        fileNames.reserve(files.size());
        for (const auto& file : files)
            fileNames.push_back(file.getFileName());

        if (fileNames == workingMidiFileNames)
        {
            refreshWorkingFileButtonsEnabledState();
            return;
        }

        const auto previousSelection = getSelectedWorkingMidiFileName();
        workingMidiFileNames = std::move(fileNames);
        workingFilesSelector.clear(juce::dontSendNotification);
        workingFilesSelector.addItem("(None)", 1);

        int selectedId = 1;
        int itemId = 2;
        for (const auto& fileName : workingMidiFileNames)
        {
            workingFilesSelector.addItem(fileName, itemId);
            if (fileName == previousSelection)
                selectedId = itemId;
            ++itemId;
        }

        workingFilesSelector.setSelectedId(selectedId, juce::dontSendNotification);
        refreshWorkingFileButtonsEnabledState();
    }

    void promptForWorkingMidiNameAndAdd(const juce::File& sourceFile,
                                        const juce::String& initialName,
                                        const juce::String& validationError)
    {
        auto* alert = new juce::AlertWindow("Add MIDI to Working Directory",
                                            validationError.isNotEmpty()
                                                ? ("Select a target MIDI filename.\n\n" + validationError)
                                                : "Select a target MIDI filename.",
                                            juce::MessageBoxIconType::QuestionIcon,
                                            this);
        alert->addTextEditor("targetName", initialName, "Filename");
        alert->addButton("Add", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PlayerTabComponent> safeThis(this);
        alert->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, alert, sourceFile](int result)
        {
            if (safeThis == nullptr || result != 1)
                return;

            const auto requested = sanitizeMidiBaseName(alert->getTextEditorContents("targetName"));
            juce::String copiedPath;
            juce::String error;
            if (!safeThis->scorePage.addMidiFileToWorkingDirectory(sourceFile, requested, copiedPath, error))
            {
                safeThis->promptForWorkingMidiNameAndAdd(sourceFile,
                                                         requested.isNotEmpty() ? requested : sourceFile.getFileNameWithoutExtension(),
                                                         error);
                return;
            }

            safeThis->refreshWorkingMidiFileList();
            safeThis->statusLabel.setText("Added MIDI: " + juce::File(copiedPath).getFileName(), juce::dontSendNotification);
        }), true);
    }

    void addMidiToWorkingDirectory()
    {
        addMidiFileChooser = std::make_unique<juce::FileChooser>(
            "Select MIDI file to copy into working directory",
            juce::File(),
            "*.mid;*.midi",
            false,
            false,
            this);
        const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
        addMidiFileChooser->launchAsync(chooserFlags, [this](const juce::FileChooser& chooser)
        {
            const auto sourceFile = chooser.getResult();
            if (!sourceFile.existsAsFile())
                return;

            promptForWorkingMidiNameAndAdd(sourceFile, sourceFile.getFileNameWithoutExtension(), {});
        });
    }

    void renameSelectedWorkingMidi()
    {
        const auto currentFileName = getSelectedWorkingMidiFileName();
        if (currentFileName.isEmpty())
            return;

        const juce::File currentFile(currentFileName);
        auto* alert = new juce::AlertWindow("Rename Working MIDI",
                                            "Enter the new filename (extension is preserved).",
                                            juce::MessageBoxIconType::QuestionIcon,
                                            this);
        alert->addTextEditor("newName", currentFile.getFileNameWithoutExtension(), "Filename");
        alert->addButton("Rename", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PlayerTabComponent> safeThis(this);
        alert->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, alert, currentFileName](int result)
        {
            if (safeThis == nullptr || result != 1)
                return;

            const auto requested = sanitizeMidiBaseName(alert->getTextEditorContents("newName"));
            juce::String renamedPath;
            juce::String error;
            if (!safeThis->scorePage.renameWorkingDirectoryMidiFile(currentFileName, requested, renamedPath, error))
            {
                safeThis->statusLabel.setText("Rename error: " + error, juce::dontSendNotification);
                return;
            }

            safeThis->refreshWorkingMidiFileList();
            safeThis->statusLabel.setText("Renamed MIDI: " + juce::File(renamedPath).getFileName(), juce::dontSendNotification);
        }), true);
    }

    void deleteSelectedWorkingMidi()
    {
        const auto currentFileName = getSelectedWorkingMidiFileName();
        if (currentFileName.isEmpty())
            return;

        auto* alert = new juce::AlertWindow("Delete Working MIDI",
                                            "Delete '" + currentFileName + "' from the working directory?",
                                            juce::MessageBoxIconType::WarningIcon,
                                            this);
        alert->addButton("Delete", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PlayerTabComponent> safeThis(this);
        alert->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, currentFileName](int result)
        {
            if (safeThis == nullptr || result != 1)
                return;

            juce::String error;
            if (!safeThis->scorePage.deleteWorkingDirectoryMidiFile(currentFileName, error))
            {
                safeThis->statusLabel.setText("Delete error: " + error, juce::dontSendNotification);
                return;
            }

            safeThis->refreshWorkingMidiFileList();
            safeThis->statusLabel.setText("Deleted MIDI: " + currentFileName, juce::dontSendNotification);
        }), true);
    }

    static juce::String sanitizeFolderName(const juce::String& input)
    {
        auto name = input.trim().removeCharacters("\\/:*?\"<>|");
        while (name.isNotEmpty() && (name.endsWithChar('.') || name.endsWithChar(' ')))
            name = name.dropLastCharacters(1);
        return name.trim();
    }

    void addWorkingSubfolder()
    {
        const juce::File parentDirectory(scorePage.getWorkingDirectoryPath());
        if (!parentDirectory.isDirectory())
        {
            statusLabel.setText("Working directory error: Select a valid working directory first.", juce::dontSendNotification);
            return;
        }

        auto* alert = new juce::AlertWindow("Add Subfolder",
                                            "Create a new folder inside:\n" + parentDirectory.getFullPathName(),
                                            juce::MessageBoxIconType::QuestionIcon,
                                            this);
        alert->addTextEditor("subfolderName", "NewFolder", "Folder name");
        alert->addButton("Create", 1, juce::KeyPress(juce::KeyPress::returnKey));
        alert->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

        juce::Component::SafePointer<PlayerTabComponent> safeThis(this);
        alert->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, alert, parentDirectory](int result)
        {
            if (safeThis == nullptr)
                return;

            if (result != 1)
                return;

            const auto requested = alert->getTextEditorContents("subfolderName");
            const auto folderName = sanitizeFolderName(requested);
            if (folderName.isEmpty())
            {
                safeThis->statusLabel.setText("Working directory error: Enter a valid folder name.", juce::dontSendNotification);
                return;
            }

            const juce::File newFolder = parentDirectory.getChildFile(folderName);
            if (newFolder.exists())
            {
                safeThis->statusLabel.setText("Working directory error: That folder already exists.", juce::dontSendNotification);
                return;
            }

            if (!newFolder.createDirectory())
            {
                safeThis->statusLabel.setText("Working directory error: Could not create folder.", juce::dontSendNotification);
                return;
            }

            safeThis->workingDirectoryInput.setText(newFolder.getFullPathName(), juce::dontSendNotification);
            safeThis->applyWorkingDirectoryInput();
        }), true);
    }

    MainComponent& scorePage;
    std::vector<MidiOutputDeviceInfo> availableOutputs;
    std::vector<juce::String> workingMidiFileNames;
    std::function<void()> exitAction;
    std::unique_ptr<juce::FileChooser> workingDirectoryChooser;
    std::unique_ptr<juce::FileChooser> addMidiFileChooser;

    juce::Label outputLabel;
    juce::ComboBox outputSelector;
    juce::TextButton refreshOutputsButton;
    juce::TextButton exitButton { "Exit" };
    juce::ToggleButton startupResumeToggle;
    juce::Label workingDirectoryLabel;
    juce::TextEditor workingDirectoryInput;
    juce::TextButton browseWorkingDirectoryButton;
    juce::TextButton addWorkingSubfolderButton;
    juce::Label workingFilesLabel;
    juce::ComboBox workingFilesSelector;
    juce::TextButton addWorkingMidiButton;
    juce::TextButton renameWorkingMidiButton;
    juce::TextButton deleteWorkingMidiButton;
    juce::ToggleButton scoreLightModeToggle;
    juce::Label fileLabel;
    juce::Label statusLabel;
};
