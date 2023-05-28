#pragma once

#include <Stream.h>

struct CommandParams; // transparent

struct QueuedCommand {
  bool queued;
  bool finished;
  CommandParams* params;
};

extern QueuedCommand queuedCommand;

void printCommandHelp(Print* out);

// Returns the success of the parse and queueing, not the result of the command
bool handleCommandAsync(char* command, Print* out);

// Returns whether the command succeeded
bool handleCommand(char* command, Print* out);
bool handleCommand(CommandParams* command);