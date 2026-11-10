#include "UiMenuInput.h"
#include "Input.h"

UiMenuInput::Command UiMenuInput::Poll(const Input *input) {
  if (!input) return Command::None;

  if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) ||
      input->WasPadPressed(XINPUT_GAMEPAD_A)) {
    return Command::Decide;
  }
  if (input->TriggerKey(DIK_UP) || input->TriggerKey(DIK_W) ||
      input->WasPadPressed(XINPUT_GAMEPAD_DPAD_UP)) {
    return Command::Up;
  }
  if (input->TriggerKey(DIK_DOWN) || input->TriggerKey(DIK_S) ||
      input->WasPadPressed(XINPUT_GAMEPAD_DPAD_DOWN)) {
    return Command::Down;
  }
  return Command::None;
}
