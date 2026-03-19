CC = gcc
CFLAGS = -Wall -Wextra
AGENT_FLAGS = -pthread
CLI_FLAGS = -pthread
BINDIR = bin

AGENT_SOURCES = agent/agent.c agent/metrics.c agent/http.c
AGENT_TARGET = $(BINDIR)/agent

CLI_SOURCES = cli/cli.c cli/nodes.c cli/table.c
CLI_TARGET = $(BINDIR)/cli

all: $(AGENT_TARGET) $(CLI_TARGET)

$(BINDIR):
	mkdir -p $(BINDIR)

$(AGENT_TARGET): $(BINDIR) $(AGENT_SOURCES)
	$(CC) $(CFLAGS) $(AGENT_FLAGS) $(AGENT_SOURCES) -o $(AGENT_TARGET)

$(CLI_TARGET): $(BINDIR) $(CLI_SOURCES)
	$(CC) $(CFLAGS) $(CLI_FLAGS) $(CLI_SOURCES) -o $(CLI_TARGET)

clean:
	rm -rf $(BINDIR)

run-agent: $(AGENT_TARGET)
	$(AGENT_TARGET)

cli: $(CLI_TARGET)
	@echo "CLI built: $(CLI_TARGET)"

run-cli: $(CLI_TARGET)
	$(CLI_TARGET)

.PHONY: all clean run-agent cli run-cli
