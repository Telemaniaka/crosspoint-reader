#pragma once
#include <string>
#include "activities/Activity.h"

class Bitmap;

class SleepActivity final : public Activity {
 public:
  explicit SleepActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, bool fromTimeout = false)
      : Activity("Sleep", renderer, mappedInput), fromTimeout(fromTimeout) {}
  void onEnter() override;
  void onScreenshotRequest() override;

 private:
  void renderDefaultSleepScreen() const;
  void renderCustomSleepScreen() const;
  void renderCoverSleepScreen() const;
  void renderBitmapSleepScreen(const Bitmap& bitmap) const;
  bool renderPxcSleepScreen(const std::string& path) const;
  void renderLastScreenSleepScreen() const;
  void renderBlankSleepScreen() const;

  // Tracks the last factory-LUT render so onScreenshotRequest() can re-render the same image.
  mutable std::string lastGrayscalePath;
  mutable bool lastGrayscaleIsPxc = false;

  bool fromTimeout = false;
};
