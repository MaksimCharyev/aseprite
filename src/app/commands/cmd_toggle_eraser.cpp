// Aseprite
// Copyright (C) 2026  Maksim Charyev
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/app.h"
#include "app/commands/command.h"
#include "app/context.h"
#include "app/tools/active_tool.h"
#include "app/tools/tool.h"
#include "app/tools/tool_box.h"
#include "app/ui/toolbar.h"

namespace app {

// Switches the selected tool to the eraser and back to the previous
// tool (e.g. bound to the pen double-tap on Android). It changes the
// selected tool (and the toolbar) instead of the eraser tip
// proximity, which the editor resets on each pen message.
class ToggleEraserCommand : public Command {
public:
  ToggleEraserCommand() : Command(CommandId::ToggleEraser()) {}

protected:
  bool onEnabled(Context* context) override
  {
    return (context->isUIAvailable() && ToolBar::instance());
  }

  bool onChecked(Context* context) override
  {
    tools::Tool* tool = App::instance()->activeToolManager()->selectedTool();
    return (tool && tool->getId() == tools::WellKnownTools::Eraser);
  }

  void onExecute(Context* context) override
  {
    tools::ToolBox* toolBox = App::instance()->toolBox();
    tools::Tool* tool = App::instance()->activeToolManager()->selectedTool();
    tools::Tool* eraser = toolBox->getToolById(tools::WellKnownTools::Eraser);
    if (!eraser)
      return;

    if (tool != eraser) {
      m_previousTool = tool;
      ToolBar::instance()->selectTool(eraser);
    }
    else {
      tools::Tool* previous = m_previousTool;
      if (!previous || previous == eraser)
        previous = toolBox->getToolById(tools::WellKnownTools::Pencil);
      if (previous)
        ToolBar::instance()->selectTool(previous);
    }
  }

private:
  // Tools are owned by the ToolBox, they live as long as the app.
  tools::Tool* m_previousTool = nullptr;
};

Command* CommandFactory::createToggleEraserCommand()
{
  return new ToggleEraserCommand;
}

} // namespace app
