#include "BmpViewerActivity.h"

#include <Bitmap.h>
#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include "components/UITheme.h"
#include "fontIds.h"

BmpViewerActivity::BmpViewerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string path)
    : Activity("BmpViewer", renderer, mappedInput), filePath(std::move(path)) {}


void BmpViewerActivity::loadSiblingFiles() {
  siblingFiles.clear();
  currentIndex = 0;

  const auto folderPath = FsHelpers::extractFolderPath(filePath);
  auto root = Storage.open(folderPath.c_str());
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    siblingFiles.push_back(filePath);
    return;
  }

  root.rewindDirectory();

  char name[500];
  for (auto file = root.openNextFile(); file; file = root.openNextFile()) {
    file.getName(name, sizeof(name));
    if (name[0] == '.' || file.isDirectory() || strcmp(name, "System Volume Information") == 0) {
      file.close();
      continue;
    }

    std::string_view filename{name};
    if (FsHelpers::hasBmpExtension(filename)) {
      if (folderPath == "/") {
        siblingFiles.emplace_back("/" + std::string(filename));
      } else {
        siblingFiles.emplace_back(folderPath + "/" + std::string(filename));
      }
    }
    file.close();
  }
  root.close();

  FsHelpers::sortFileList(siblingFiles);

  currentIndex = findEntry(filePath);
}

size_t BmpViewerActivity::findEntry(const std::string& name) const {
  for (size_t i = 0; i < siblingFiles.size(); i++)
    if (siblingFiles[i] == name) return i;
  return 0;
}

bool BmpViewerActivity::selectAdjacentFile(int direction) {
  if (siblingFiles.size() <= 1) {
    return false;
  }

  const auto size = static_cast<int>(siblingFiles.size());
  const auto nextIndex = (static_cast<int>(currentIndex) + direction + size) % size;
  if (nextIndex == static_cast<int>(currentIndex)) {
    return false;
  }

  currentIndex = static_cast<size_t>(nextIndex);
  filePath = siblingFiles[currentIndex];
  return true;
}

void BmpViewerActivity::onEnter() {
  Activity::onEnter();
  loadSiblingFiles();
  renderCurrentImage();
}

void BmpViewerActivity::renderCurrentImage() const {
  // Removed the redundant initial renderer.clearScreen()

  FsFile file;

  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  Rect popupRect = GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
  GUI.fillPopupProgress(renderer, popupRect, 20);  // Initial 20% progress
  // 1. Open the file
  if (Storage.openFileForRead("BMP", filePath, file)) {
    Bitmap bitmap(file, true);

    // 2. Parse headers to get dimensions
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      int x, y;

      if (bitmap.getWidth() > pageWidth || bitmap.getHeight() > pageHeight) {
        float ratio = static_cast<float>(bitmap.getWidth()) / static_cast<float>(bitmap.getHeight());
        const float screenRatio = static_cast<float>(pageWidth) / static_cast<float>(pageHeight);

        if (ratio > screenRatio) {
          // Wider than screen
          x = 0;
          y = std::round((static_cast<float>(pageHeight) - static_cast<float>(pageWidth) / ratio) / 2);
        } else {
          // Taller than screen
          x = std::round((static_cast<float>(pageWidth) - static_cast<float>(pageHeight) * ratio) / 2);
          y = 0;
        }
      } else {
        // Center small images
        x = (pageWidth - bitmap.getWidth()) / 2;
        y = (pageHeight - bitmap.getHeight()) / 2;
      }

      // 4. Prepare Rendering
      const bool hasAdjacentFiles = siblingFiles.size() > 1;
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", hasAdjacentFiles ? tr(STR_DIR_UP) : "",
                                                hasAdjacentFiles ? tr(STR_DIR_DOWN) : "");
      GUI.fillPopupProgress(renderer, popupRect, 50);

      renderer.clearScreen();
      // Assuming drawBitmap defaults to 0,0 crop if omitted, or pass explicitly: drawBitmap(bitmap, x, y, pageWidth,
      // pageHeight, 0, 0)
      renderer.drawBitmap(bitmap, x, y, pageWidth, pageHeight, 0, 0);

      // Draw UI hints on the base layer
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      // Single pass for non-grayscale images

      renderer.displayBuffer(HalDisplay::FULL_REFRESH);

    } else {
      // Handle file parsing error
      renderer.clearScreen();
      renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "Invalid BMP File");
      const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
      GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
      renderer.displayBuffer(HalDisplay::FAST_REFRESH);
    }

    file.close();
  } else {
    // Handle file open error
    renderer.clearScreen();
    renderer.drawCenteredText(UI_10_FONT_ID, pageHeight / 2, "Could not open file");
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
    renderer.displayBuffer(HalDisplay::FULL_REFRESH);
  }
}

void BmpViewerActivity::onExit() {
  Activity::onExit();
  renderer.clearScreen();
  renderer.displayBuffer(HalDisplay::FAST_REFRESH);
}

void BmpViewerActivity::loop() {
  // Keep CPU awake/polling so 1st click works
  Activity::loop();

  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    activityManager.goToFileBrowser(filePath);
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Left) || mappedInput.wasReleased(MappedInputManager::Button::Up)) {
    if (selectAdjacentFile(-1)) {
      renderCurrentImage();
    }
    return;
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Right) || mappedInput.wasReleased(MappedInputManager::Button::Down)) {
    if (selectAdjacentFile(1)) {
      renderCurrentImage();
    }
    return;
  }
}