#include "mainMenu.hpp"
#include "../audio.hpp"
#include "../image.hpp"
#include "../input.hpp"
#include "../interpret.hpp"
#include "../render.hpp"
#include "../text.hpp"
#include "../unzip.hpp"
#include <nlohmann/json.hpp>
#ifdef __WIIU__
#include <whb/sdcard.h>
#endif
#include "../curl.hpp"
MainMenu::MainMenu() {
    init();
}
MainMenu::~MainMenu() {
    cleanup();
}

bool MainMenu::activateMainMenu() {

    if (projectType != UNEMBEDDED) return false;

    Render::renderMode = Render::BOTH_SCREENS;

    MainMenu menu;
    bool isLoaded = false;
    while (!isLoaded) {
        menu.render();

        if (menu.loadButton->isPressed()) {
            menu.cleanup();
            ProjectMenu projectMenu;

            while (!projectMenu.shouldGoBack) {
                projectMenu.render();

                if (!Render::appShouldRun()) {
                    Log::logWarning("App should close. cleaning up menu.");
                    projectMenu.cleanup();
                    menu.shouldExit = true;
                    projectMenu.shouldGoBack = true;
                }
            }
            projectMenu.cleanup();
            menu.init();
        }

        if (!Render::appShouldRun() || menu.shouldExit) {
            menu.cleanup();
            Log::logWarning("app should exit. closing app.");
            return false;
        }

        if (Unzip::filePath != "") {
            menu.cleanup();
            Image::cleanupImages();
            SoundPlayer::cleanupAudio();
            if (!Unzip::load()) {
                Log::logWarning("Could not load project. closing app.");
                return false;
            }
            isLoaded = true;
        }
    }
    if (!Render::appShouldRun()) {
        Log::logWarning("app should exit. closing app.");
        menu.cleanup();
        return false;
    }
    return true;
}

void MainMenu::init() {

    Input::applyControls();

    logo = new MenuImage("gfx/logo.png");
    logo->x = 200;

    logoStartTime.start();

    loadButton = new ButtonObject("", "gfx/menu/play.png", 200, 180);
    loadButton->isSelected = true;
    settingsButton = new ButtonObject("", "gfx/menu/settings.png", 300, 180);
    mainMenuControl = new ControlObject();
    mainMenuControl->selectedObject = loadButton;
    loadButton->buttonRight = settingsButton;
    settingsButton->buttonLeft = loadButton;
    mainMenuControl->buttonObjects.push_back(loadButton);
    mainMenuControl->buttonObjects.push_back(settingsButton);
}

void MainMenu::render() {

    Input::getInput();
    mainMenuControl->input();

    // begin frame
    Render::beginFrame(0, 117, 77, 117);

    // move and render logo
    const float elapsed = logoStartTime.getTimeMs();
    float bobbingOffset = std::sin(elapsed * 0.0025f) * 5.0f;
    logo->y = 75 + bobbingOffset;
    logo->render();

    // begin 3DS bottom screen frame
    Render::beginFrame(1, 117, 77, 117);

    if (settingsButton->isPressed()) {
        settingsButton->x += 100;
    }

    loadButton->render();
    settingsButton->render();
    mainMenuControl->render();

    Render::endFrame();
}
void MainMenu::cleanup() {

    if (logo) {
        delete logo;
        logo = nullptr;
    }
    if (loadButton) {
        delete loadButton;
        loadButton = nullptr;
    }
    if (settingsButton) {
        delete settingsButton;
        settingsButton = nullptr;
    }
    if (mainMenuControl) {
        delete mainMenuControl;
        mainMenuControl = nullptr;
    }
}

ProjectMenu::ProjectMenu() {
    init();
}

ProjectMenu::~ProjectMenu() {
    cleanup();
}

void ProjectMenu::init() {

    projectControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.png", 375, 20);
    backButton->needsToBeSelected = false;
    backButton->scale = 1.0;

    std::vector<std::string> projectFiles;
    projectFiles = Unzip::getProjectFiles(OS::getScratchFolderLocation());

    // initialize text and set positions
    int yPosition = 120;
    for (std::string &file : projectFiles) {
        ButtonObject *project = new ButtonObject(file.substr(0, file.length() - 4), "gfx/menu/projectBox.png", 0, yPosition);
        project->text->setColor(Math::color(0, 0, 0, 255));
        project->canBeClicked = false;
        project->y -= project->text->getSize()[1] / 2;
        if (project->text->getSize()[0] > project->buttonTexture->image->getWidth() * 0.85) {
            float scale = (float)project->buttonTexture->image->getWidth() / (project->text->getSize()[0] * 1.15);
            project->textScale = scale;
        }
        projects.push_back(project);
        projectControl->buttonObjects.push_back(project);
        yPosition += 50;
    }
    for (size_t i = 0; i < projects.size(); i++) {
        // Check if there's a project above
        if (i > 0) {
            projects[i]->buttonUp = projects[i - 1];
        }

        // Check if there's a project below
        if (i < projects.size() - 1) {
            projects[i]->buttonDown = projects[i + 1];
        }
    }

    // check if user has any projects
    if (projectFiles.size() == 0) {
        hasProjects = false;
        noProjectsButton = new ButtonObject("", "gfx/menu/noProjects.png", 200, 120);
        projectControl->selectedObject = noProjectsButton;
        projectControl->selectedObject->isSelected = true;
        noProjectsText = createTextObject("No Scratch projects found!", 0, 0);
        noProjectsText->setCenterAligned(true);
        noProjectInfo = createTextObject("a", 0, 0);
        noProjectInfo->setCenterAligned(true);

#ifdef __WIIU__
        noProjectInfo->setText("Put Scratch projects in sd:/wiiu/scratch-wiiu/ !");
#elif defined(__3DS__)
        noProjectInfo->setText("Project location has moved to sd:/3ds/scratch-everywhere !");
#elif defined(WII)
        noProjectInfo->setText("Put Scratch projects in sd:/apps/scratch-wii !");
#elif defined(VITA)
        noProjectInfo->setText("Put Scratch projects in ux0:data/scratch-vita/ ! If the folder doesn't exist, create it.");
#else
        noProjectInfo->setText("Put Scratch projects in the same folder as the app!");
#endif

        if (noProjectInfo->getSize()[0] > Render::getWidth() * 0.85) {
            float scale = (float)Render::getWidth() / (noProjectInfo->getSize()[0] * 1.15);
            noProjectInfo->setScale(scale);
        }
        if (noProjectsText->getSize()[0] > Render::getWidth() * 0.85) {
            float scale = (float)Render::getWidth() / (noProjectsText->getSize()[0] * 1.15);
            noProjectsText->setScale(scale);
        }

    } else {
        projectControl->selectedObject = projects.front();
        projectControl->selectedObject->isSelected = true;
        cameraY = projectControl->selectedObject->y;
        hasProjects = true;

        playButton = new ButtonObject("Play (A)", "gfx/menu/optionBox.svg", 95, 230);
        settingsButton = new ButtonObject("Settings (R)", "gfx/menu/optionBox.svg", 315, 230);
        downloadButton = new ButtonObject("Download Project (L)", "gfx/menu/optionBox.svg", 315, 210);

        playButton->scale = 0.6;
        settingsButton->scale = 0.6;
        downloadButton->scale = 0.6;

        playButton->needsToBeSelected = false;
        settingsButton->needsToBeSelected = false;
        downloadButton->needsToBeSelected = false;
    }
}

void ProjectMenu::render() {
    Input::getInput();
    projectControl->input();

    float targetY = 0.0f;
    float lerpSpeed = 0.1f;

    if (hasProjects) {
        if (projectControl->selectedObject->isPressed({"a"}) || playButton->isPressed({"a"})) {
            Unzip::filePath = projectControl->selectedObject->text->getText() + ".sb3";
            shouldGoBack = true;
            return;
        }
        if (settingsButton->isPressed({"r"})) {
            std::string selectedProject = projectControl->selectedObject->text->getText();
            cleanup();
            ProjectSettings settings(selectedProject);
            while (settings.shouldGoBack == false && Render::appShouldRun()) {
                settings.render();
            }
            settings.cleanup();
            init();
        }
        if (downloadButton->isPressed({"l"})) {
            std::string selectedProject = projectControl->selectedObject->text->getText();
            cleanup();
            ProjectDownload download(selectedProject);
            while (download.shouldGoBack == false && Render::appShouldRun()) {
                download.render();
            }
            download.cleanup();
            init();
        }
        targetY = projectControl->selectedObject->y;
        lerpSpeed = 0.1f;
    } else {
        if (noProjectsButton->isPressed({"a"})) {
            shouldGoBack = true;
            return;
        }
    }

    if (backButton->isPressed({"b", "y"})) {
        shouldGoBack = true;
    }

    cameraY = cameraY + (targetY - cameraY) * lerpSpeed;
    cameraX = 200;
    const double cameraYOffset = 110;

    Render::beginFrame(0, 108, 100, 128);
    Render::beginFrame(1, 108, 100, 128);

    for (ButtonObject *project : projects) {
        if (project == nullptr) continue;

        if (projectControl->selectedObject == project)
            project->text->setColor(Math::color(32, 36, 41, 255));
        else
            project->text->setColor(Math::color(0, 0, 0, 255));

        const double xPos = project->x + cameraX;
        const double yPos = project->y - (cameraY - cameraYOffset);

        // Calculate target scale based on distance
        const double distance = abs(project->y - targetY);
        const int maxDistance = 500;
        float targetScale;
        if (distance <= maxDistance) {
            targetScale = 1.0f - (distance / static_cast<float>(maxDistance));

            // Lerp the scale towards the target scale
            project->scale = project->scale + (targetScale - project->scale) * lerpSpeed;

            project->render(xPos, yPos);

        } else {
            targetScale = 0.0f;
        }
    }
    backButton->render();
    if (hasProjects) {
        playButton->render();
        settingsButton->render();
        downloadButton->render();
        projectControl->render(cameraX, cameraY - cameraYOffset);
    } else {
        noProjectsButton->render();
        noProjectsText->render(Render::getWidth() / 2, Render::getHeight() * 0.75);
        noProjectInfo->render(Render::getWidth() / 2, Render::getHeight() * 0.85);
        projectControl->render();
    }

    Render::endFrame();
}

void ProjectMenu::cleanup() {
    for (ButtonObject *button : projects) {
        delete button;
    }
    if (projectControl != nullptr) {
        delete projectControl;
        projectControl = nullptr;
    }
    projects.clear();
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    if (playButton != nullptr) {
        delete playButton;
        playButton = nullptr;
    }
    if (settingsButton != nullptr) {
        delete settingsButton;
        settingsButton = nullptr;
    }
    if (noProjectsButton != nullptr) {
        delete noProjectsButton;
        noProjectsButton = nullptr;
    }
    if (noProjectsText != nullptr) {
        delete noProjectsText;
        noProjectsText = nullptr;
    }
    if (noProjectInfo != nullptr) {
        delete noProjectInfo;
        noProjectInfo = nullptr;
    }
    Render::beginFrame(0, 108, 100, 128);
    Render::beginFrame(1, 108, 100, 128);
    Input::getInput();
    Render::endFrame();
}

ProjectSettings::ProjectSettings(std::string projPath) {
    projectPath = projPath;
    init();
}
ProjectSettings::~ProjectSettings() {
    cleanup();
}

void ProjectSettings::init() {
    // initialize
    changeControlsButton = new ButtonObject("Change Controls", "gfx/menu/projectBox.png", 200, 100);
    changeControlsButton->text->setColor(Math::color(0, 0, 0, 255));
    bottomScreenButton = new ButtonObject("Bottom Screen", "gfx/menu/projectBox.png", 200, 150);
    bottomScreenButton->text->setColor(Math::color(0, 0, 0, 255));
    settingsControl = new ControlObject();
    backButton = new ButtonObject("", "gfx/menu/buttonBack.png", 375, 20);
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;

    // initial selected object
    settingsControl->selectedObject = changeControlsButton;
    changeControlsButton->isSelected = true;

    // link buttons
    changeControlsButton->buttonDown = bottomScreenButton;
    changeControlsButton->buttonUp = bottomScreenButton;
    bottomScreenButton->buttonUp = changeControlsButton;
    bottomScreenButton->buttonDown = changeControlsButton;

    // add buttons to control
    settingsControl->buttonObjects.push_back(changeControlsButton);
    settingsControl->buttonObjects.push_back(bottomScreenButton);
}
void ProjectSettings::render() {
    Input::getInput();
    settingsControl->input();

    if (changeControlsButton->isPressed({"a"})) {
        cleanup();
        ControlsMenu controlsMenu(projectPath);
        while (controlsMenu.shouldGoBack == false && Render::appShouldRun()) {
            controlsMenu.render();
        }
        controlsMenu.cleanup();
        init();
    }
    // if (bottomScreenButton->isPressed()) {
    // }
    if (backButton->isPressed({"b", "y"})) {
        shouldGoBack = true;
    }

    Render::beginFrame(0, 147, 138, 168);
    Render::beginFrame(1, 147, 138, 168);

    changeControlsButton->render();
    // bottomScreenButton->render();
    settingsControl->render();
    backButton->render();

    Render::endFrame();
}
void ProjectSettings::cleanup() {
    if (changeControlsButton != nullptr) {
        delete changeControlsButton;
        changeControlsButton = nullptr;
    }
    if (bottomScreenButton != nullptr) {
        delete bottomScreenButton;
        bottomScreenButton = nullptr;
    }
    if (settingsControl != nullptr) {
        delete settingsControl;
        settingsControl = nullptr;
    }
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    Render::beginFrame(0, 147, 138, 168);
    Render::beginFrame(1, 147, 138, 168);
    Input::getInput();
    Render::endFrame();
}

ControlsMenu::ControlsMenu(std::string projPath) {
    projectPath = projPath;
    init();
}

ControlsMenu::~ControlsMenu() {
    cleanup();
}

void ControlsMenu::init() {

    Unzip::filePath = projectPath + ".sb3";
    if (!Unzip::load()) {
        Log::logError("Failed to load project for ControlsMenu.");
        toExit = true;
        return;
    }
    Unzip::filePath = "";

    // get controls
    std::vector<std::string> controls;

    for (auto &sprite : sprites) {
        for (auto &[id, block] : sprite->blocks) {
            std::string buttonCheck;
            if (block.opcode == "sensing_keypressed") {

                // stolen code from sensing.cpp

                auto inputFind = block.parsedInputs.find("KEY_OPTION");
                // if no variable block is in the input
                if (inputFind->second.inputType == ParsedInput::LITERAL) {
                    Block *inputBlock = findBlock(inputFind->second.literalValue.asString());
                    if (!inputBlock->fields["KEY_OPTION"][0].is_null())
                        buttonCheck = inputBlock->fields["KEY_OPTION"][0];
                } else {
                    buttonCheck = Scratch::getInputValue(block, "KEY_OPTION", sprite).asString();
                }

            } else if (block.opcode == "event_whenkeypressed") {
                buttonCheck = block.fields.at("KEY_OPTION")[0];
            } else continue;
            if (buttonCheck != "" && std::find(controls.begin(), controls.end(), buttonCheck) == controls.end()) {
                Log::log("Found new control: " + buttonCheck);
                controls.push_back(buttonCheck);
            }
        }
    }

    Scratch::cleanupScratchProject();
    Render::renderMode = Render::BOTH_SCREENS;

    settingsControl = new ControlObject();
    settingsControl->selectedObject = nullptr;
    backButton = new ButtonObject("", "gfx/menu/buttonBack.png", 375, 20);
    applyButton = new ButtonObject("Apply (Y)", "gfx/menu/optionBox.svg", 200, 230);
    applyButton->scale = 0.6;
    applyButton->needsToBeSelected = false;
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;

    if (controls.empty()) {
        Log::logWarning("No controls found in project");
        shouldGoBack = true;
        return;
    }

    double yPosition = 100;
    for (auto &control : controls) {
        key newControl;
        ButtonObject *controlButton = new ButtonObject(control, "gfx/menu/optionBox.svg", 0, yPosition);
        controlButton->text->setColor(Math::color(255, 255, 255, 255));
        controlButton->scale = 0.6;
        controlButton->y -= controlButton->text->getSize()[1] / 2;
        if (controlButton->text->getSize()[0] > controlButton->buttonTexture->image->getWidth() * 0.3) {
            float scale = (float)controlButton->buttonTexture->image->getWidth() / (controlButton->text->getSize()[0] * 3);
            controlButton->textScale = scale;
        }
        controlButton->canBeClicked = false;
        newControl.button = controlButton;
        newControl.control = control;

        for (const auto &pair : Input::inputControls) {
            if (pair.second == newControl.control) {
                newControl.controlValue = pair.first;
                break;
            }
        }

        controlButtons.push_back(newControl);
        settingsControl->buttonObjects.push_back(controlButton);
        yPosition += 50;
    }
    if (!controls.empty()) {
        settingsControl->selectedObject = controlButtons.front().button;
        settingsControl->selectedObject->isSelected = true;
        cameraY = settingsControl->selectedObject->y;
    }

    // link buttons
    for (size_t i = 0; i < controlButtons.size(); i++) {
        if (i > 0) {
            controlButtons[i].button->buttonUp = controlButtons[i - 1].button;
        }
        if (i < controlButtons.size() - 1) {
            controlButtons[i].button->buttonDown = controlButtons[i + 1].button;
        }
    }

    Input::applyControls();
}

void ControlsMenu::render() {
    Input::getInput();
    settingsControl->input();

    if (backButton->isPressed({"b"})) {
        shouldGoBack = true;
        return;
    }
    if (applyButton->isPressed({"y"})) {
        applyControls();
        shouldGoBack = true;
        return;
    }

    if (settingsControl->selectedObject->isPressed()) {
        Input::keyHeldFrames = -999;

        // wait till A isnt pressed
        while (!Input::inputButtons.empty() && Render::appShouldRun()) {
            Input::getInput();
        }

        while (Input::keyHeldFrames < 2 && Render::appShouldRun()) {
            Input::getInput();
        }
        if (!Input::inputButtons.empty()) {

            // remove "any" first
            auto it = std::find(Input::inputButtons.begin(), Input::inputButtons.end(), "any");
            if (it != Input::inputButtons.end()) {
                Input::inputButtons.erase(it);
            }

            std::string key = Input::inputButtons.back();
            for (const auto &pair : Input::inputControls) {
                if (pair.second == key) {
                    // Update the control value
                    for (auto &newControl : controlButtons) {
                        if (newControl.button == settingsControl->selectedObject) {
                            newControl.controlValue = pair.first;
                            Log::log("Updated control: " + newControl.control + " -> " + newControl.controlValue);
                            break;
                        }
                    }
                    break;
                }
            }
        } else {
            Log::logWarning("No input detected for control assignment.");
        }
    }

    // Smooth camera movement to follow selected control
    const float targetY = settingsControl->selectedObject->y;
    const float lerpSpeed = 0.1f;

    cameraY = cameraY + (targetY - cameraY) * lerpSpeed;
    const int cameraX = 200;
    const double cameraYOffset = 110;

    Render::beginFrame(0, 181, 165, 111);
    Render::beginFrame(1, 181, 165, 111);

    for (key &controlButton : controlButtons) {
        if (controlButton.button == nullptr) continue;

        // Update button text
        controlButton.button->text->setText(
            controlButton.control + " = " + controlButton.controlValue);

        // Highlight selected
        if (settingsControl->selectedObject == controlButton.button)
            controlButton.button->text->setColor(Math::color(0, 0, 0, 255));
        else
            controlButton.button->text->setColor(Math::color(0, 0, 0, 255));

        // Position with camera offset
        const double xPos = controlButton.button->x + cameraX;
        const double yPos = controlButton.button->y - (cameraY - cameraYOffset);

        // Scale based on distance to selected
        const double distance = abs(controlButton.button->y - targetY);
        const int maxDistance = 500;
        float targetScale;
        if (distance <= maxDistance) {
            targetScale = 1.0f - (distance / static_cast<float>(maxDistance));
        } else {
            targetScale = 0.0f;
        }

        // Smooth scaling
        controlButton.button->scale = controlButton.button->scale + (targetScale - controlButton.button->scale) * lerpSpeed;

        controlButton.button->render(xPos, yPos);
    }

    // Render UI elements
    settingsControl->render(cameraX, cameraY - cameraYOffset);
    backButton->render();
    applyButton->render();

    Render::endFrame();
}

void ControlsMenu::applyControls() {
    // Build the file path
    std::string folderPath = OS::getScratchFolderLocation() + projectPath;
    std::string filePath = folderPath + ".sb3" + ".json";

    // Make sure parent directories exist
    try {
        std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
    } catch (const std::filesystem::filesystem_error &e) {
        Log::logError("Failed to create directories: " + std::string(e.what()));
        return;
    }

    // Create a JSON object to hold control mappings
    nlohmann::json json;
    json["controls"] = nlohmann::json::object();

    // Save each control in the form: "ControlName": "MappedKey"
    for (const auto &c : controlButtons) {
        json["controls"][c.control] = c.controlValue;
    }

    // Write JSON to file (overwrite if exists)
    std::ofstream file(filePath);
    if (!file) {
        Log::logError("Failed to create JSON file: " + filePath);
        return;
    }
    file << json.dump(2);
    file.close();

    Log::log("Controls saved to: " + filePath);
}

void ControlsMenu::cleanup() {
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    for (key button : controlButtons) {
        delete button.button;
    }
    controlButtons.clear();
    if (settingsControl != nullptr) {
        delete settingsControl;
        settingsControl = nullptr;
    }
    if (applyButton != nullptr) {
        delete applyButton;
        applyButton = nullptr;
    }
    Render::beginFrame(0, 181, 165, 111);
    Render::beginFrame(1, 181, 165, 111);
    Input::getInput();
    Render::endFrame();
    Render::renderMode = Render::BOTH_SCREENS;
}

ProjectDownload::ProjectDownload(std::string projPath) {
    init();
}
ProjectDownload::~ProjectDownload() {
    cleanup();
}

void ProjectDownload::init() {
    // initialize
    pinpad1 = new ButtonObject("1", "gfx/menu/buttonPinpad.png", 50, 50);
    pinpad1->text->setColor(Math::color(0, 0, 0, 255));
    pinpad1->scale = 1.5;
    pinpad2 = new ButtonObject("2", "gfx/menu/buttonPinpad.png", 90, 50);
    pinpad2->text->setColor(Math::color(0, 0, 0, 255));
    pinpad2->scale = 1.5;
    pinpad3 = new ButtonObject("3", "gfx/menu/buttonPinpad.png", 130, 50);
    pinpad3->text->setColor(Math::color(0, 0, 0, 255));
    pinpad3->scale = 1.5;
    pinpad4 = new ButtonObject("4", "gfx/menu/buttonPinpad.png", 50, 90);
    pinpad4->text->setColor(Math::color(0, 0, 0, 255));
    pinpad4->scale = 1.5;
    pinpad5 = new ButtonObject("5", "gfx/menu/buttonPinpad.png", 90, 90);
    pinpad5->text->setColor(Math::color(0, 0, 0, 255));
    pinpad5->scale = 1.5;
    pinpad6 = new ButtonObject("6", "gfx/menu/buttonPinpad.png", 130, 90);
    pinpad6->text->setColor(Math::color(0, 0, 0, 255));
    pinpad6->scale = 1.5;
    pinpad7 = new ButtonObject("7", "gfx/menu/buttonPinpad.png", 50, 130);
    pinpad7->text->setColor(Math::color(0, 0, 0, 255));
    pinpad7->scale = 1.5;
    pinpad8 = new ButtonObject("8", "gfx/menu/buttonPinpad.png", 90, 130);
    pinpad8->text->setColor(Math::color(0, 0, 0, 255));
    pinpad8->scale = 1.5;
    pinpad9 = new ButtonObject("9", "gfx/menu/buttonPinpad.png", 130, 130);
    pinpad9->text->setColor(Math::color(0, 0, 0, 255));
    pinpad9->scale = 1.5;
    backspaceButton = new ButtonObject("<", "gfx/menu/buttonPinpad.png", 50, 170);
    backspaceButton->text->setColor(Math::color(0, 0, 0, 255));
    backspaceButton->scale = 1.5;
    pinpad0 = new ButtonObject("0", "gfx/menu/buttonPinpad.png", 90, 170);
    pinpad0->text->setColor(Math::color(0, 0, 0, 255));
    pinpad0->scale = 1.5;
    okButton = new ButtonObject("OK", "gfx/menu/buttonPinpad.png", 130, 170);
    okButton->text->setColor(Math::color(0, 0, 0, 255));
    okButton->scale = 1.5;
    backButton = new ButtonObject("", "gfx/menu/buttonBack.png", 375, 20);
    downloadControl = new ControlObject();
    backButton->scale = 1.0;
    backButton->needsToBeSelected = false;
    projectId = createTextObject("Project ID", 300, 50);
    projectId->setCenterAligned(true);
    projectId->setScale(1.7);

    // initial selected object
    downloadControl->selectedObject = pinpad1;
    pinpad1->isSelected = true;

    // link buttons
    pinpad1->buttonUp = backspaceButton;
    pinpad1->buttonRight = pinpad2;
    pinpad1->buttonDown = pinpad4;
    pinpad1->buttonLeft = pinpad3;

    pinpad2->buttonUp = pinpad0;
    pinpad2->buttonRight = pinpad3;
    pinpad2->buttonDown = pinpad5;
    pinpad2->buttonLeft = pinpad1;

    pinpad3->buttonUp = okButton;
    pinpad3->buttonRight = pinpad1;
    pinpad3->buttonDown = pinpad6;
    pinpad3->buttonLeft = pinpad2;

    pinpad4->buttonUp = pinpad1;
    pinpad4->buttonRight = pinpad5;
    pinpad4->buttonDown = pinpad7;
    pinpad4->buttonLeft = pinpad6;

    pinpad5->buttonUp = pinpad2;
    pinpad5->buttonRight = pinpad6;
    pinpad5->buttonDown = pinpad8;
    pinpad5->buttonLeft = pinpad4;

    pinpad6->buttonUp = pinpad3;
    pinpad6->buttonRight = pinpad4;
    pinpad6->buttonDown = pinpad9;
    pinpad6->buttonLeft = pinpad5;

    pinpad7->buttonUp = pinpad4;
    pinpad7->buttonRight = pinpad8;
    pinpad7->buttonDown = backspaceButton;
    pinpad7->buttonLeft = pinpad9;

    pinpad8->buttonUp = pinpad5;
    pinpad8->buttonRight = pinpad9;
    pinpad8->buttonDown = pinpad0;
    pinpad8->buttonLeft = pinpad7;

    pinpad9->buttonUp = pinpad6;
    pinpad9->buttonRight = pinpad7;
    pinpad9->buttonDown = okButton;
    pinpad9->buttonLeft = pinpad8;

    backspaceButton->buttonUp = pinpad7;
    backspaceButton->buttonRight = pinpad0;
    backspaceButton->buttonDown = pinpad1;
    backspaceButton->buttonLeft = okButton;

    pinpad0->buttonUp = pinpad8;
    pinpad0->buttonRight = okButton;
    pinpad0->buttonDown = pinpad2;
    pinpad0->buttonLeft = backspaceButton;

    okButton->buttonUp = pinpad9;
    okButton->buttonRight = backspaceButton;
    okButton->buttonDown = pinpad3;
    okButton->buttonLeft = pinpad0;

    // add buttons to control
    downloadControl->buttonObjects.push_back(pinpad1);
    downloadControl->buttonObjects.push_back(pinpad2);
    downloadControl->buttonObjects.push_back(pinpad3);
    downloadControl->buttonObjects.push_back(pinpad4);
    downloadControl->buttonObjects.push_back(pinpad5);
    downloadControl->buttonObjects.push_back(pinpad6);
    downloadControl->buttonObjects.push_back(pinpad7);
    downloadControl->buttonObjects.push_back(pinpad8);
    downloadControl->buttonObjects.push_back(pinpad9);
    downloadControl->buttonObjects.push_back(pinpad0);
    downloadControl->buttonObjects.push_back(backspaceButton);
    downloadControl->buttonObjects.push_back(okButton);
}
void ProjectDownload::render() {
    Input::getInput();
    downloadControl->input();
    if (pinpad1->isSelected) pinpad1->scale = 1.7;
    else pinpad1->scale = 1.5;
    if (pinpad2->isSelected) pinpad2->scale = 1.7;
    else pinpad2->scale = 1.5;
    if (pinpad3->isSelected) pinpad3->scale = 1.7;
    else pinpad3->scale = 1.5;
    if (pinpad4->isSelected) pinpad4->scale = 1.7;
    else pinpad4->scale = 1.5;
    if (pinpad5->isSelected) pinpad5->scale = 1.7;
    else pinpad5->scale = 1.5;
    if (pinpad6->isSelected) pinpad6->scale = 1.7;
    else pinpad6->scale = 1.5;
    if (pinpad7->isSelected) pinpad7->scale = 1.7;
    else pinpad7->scale = 1.5;
    if (pinpad8->isSelected) pinpad8->scale = 1.7;
    else pinpad8->scale = 1.5;
    if (pinpad9->isSelected) pinpad9->scale = 1.7;
    else pinpad9->scale = 1.5;
    if (backspaceButton->isSelected) backspaceButton->scale = 1.7;
    else backspaceButton->scale = 1.5;
    if (pinpad0->isSelected) pinpad0->scale = 1.7;
    else pinpad0->scale = 1.5;
    if (okButton->isSelected) okButton->scale = 1.7;
    else okButton->scale = 1.5;

    if (pinpad1->isPressed({"a"}) && pinpadInput.length() < 13) pinpadInput += "1";
    if (pinpad2->isPressed({"a"}) && pinpadInput.length() < 13) pinpadInput += "2";
    if (pinpad3->isPressed({"a"}) && pinpadInput.length() < 13) pinpadInput += "3";
    if (pinpad4->isPressed({"a"}) && pinpadInput.length() < 13) pinpadInput += "4";
    if (pinpad5->isPressed({"a"}) && pinpadInput.length() < 13) pinpadInput += "5";
    if (pinpad6->isPressed({"a"}) && pinpadInput.length() < 13) pinpadInput += "6";
    if (pinpad7->isPressed({"a"}) && pinpadInput.length() < 13) pinpadInput += "7";
    if (pinpad8->isPressed({"a"}) && pinpadInput.length() < 13) pinpadInput += "8";
    if (pinpad9->isPressed({"a"}) && pinpadInput.length() < 13) pinpadInput += "9";
    if (pinpad0->isPressed({"a"}) && pinpadInput.length() < 13) pinpadInput += "0";
    if (backspaceButton->isPressed({"a"}) && pinpadInput.length() > 0) {
        std::string backspacing = "";
        for (unsigned int i = 0; i < pinpadInput.length() - 1; i++) {
            backspacing += pinpadInput[i];
        }
        pinpadInput = backspacing;
    }
    if (okButton->isPressed({"a"})) {
        // Look up the project ID
        // #ifdef __WIIU__
            try {
                pinpadInput = Curl::FetchJson("http://httpbin.org/get"/*"http://api.scratch.mit.edu/projects/" + pinpadInput*/);
                if (pinpadInput == "") pinpadInput = "BLANK";
            } catch (...) {
                pinpadInput = "ERROR";
            }
        // #endif
    }
    if (backButton->isPressed({"b", "y"})) shouldGoBack = true;

    if (pinpadInput.length() > 0) projectId->setText(pinpadInput);
    else projectId->setText("Project ID");

    Render::beginFrame(0, 147, 138, 168);
    Render::beginFrame(1, 147, 138, 168);

    pinpad1->render();
    pinpad2->render();
    pinpad3->render();
    pinpad4->render();
    pinpad5->render();
    pinpad6->render();
    pinpad7->render();
    pinpad8->render();
    pinpad9->render();
    pinpad0->render();
    backspaceButton->render();
    okButton->render();
    backButton->render();
    projectId->render(Render::getWidth() * 0.75, Render::getHeight() * 0.2);

    Render::endFrame();
}
void ProjectDownload::cleanup() {
    if (pinpad1 != nullptr) {
        delete pinpad1;
        pinpad1 = nullptr;
    }
    if (pinpad2 != nullptr) {
        delete pinpad2;
        pinpad2 = nullptr;
    }
    if (pinpad3 != nullptr) {
        delete pinpad3;
        pinpad3 = nullptr;
    }
    if (pinpad4 != nullptr) {
        delete pinpad4;
        pinpad4 = nullptr;
    }
    if (pinpad5 != nullptr) {
        delete pinpad5;
        pinpad5 = nullptr;
    }
    if (pinpad6 != nullptr) {
        delete pinpad6;
        pinpad6 = nullptr;
    }
    if (pinpad7 != nullptr) {
        delete pinpad7;
        pinpad7 = nullptr;
    }
    if (pinpad8 != nullptr) {
        delete pinpad8;
        pinpad8 = nullptr;
    }
    if (pinpad9 != nullptr) {
        delete pinpad9;
        pinpad9 = nullptr;
    }
    if (pinpad0 != nullptr) {
        delete pinpad0;
        pinpad0 = nullptr;
    }
    if (backspaceButton != nullptr) {
        delete backspaceButton;
        backspaceButton = nullptr;
    }
    if (okButton != nullptr) {
        delete okButton;
        okButton = nullptr;
    }
    if (backButton != nullptr) {
        delete backButton;
        backButton = nullptr;
    }
    Render::beginFrame(0, 147, 138, 168);
    Render::beginFrame(1, 147, 138, 168);
    Input::getInput();
    Render::endFrame();
}
