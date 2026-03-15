CC = gcc
CFLAGS = -Wall -Wextra
BINDIR = bin

AGENT_SOURCES = agent/agent.c agent/metrics.c agent/http.c
AGENT_TARGET = $(BINDIR)/nodestatus-agent

all: $(AGENT_TARGET)

$(BINDIR):
	mkdir -p $(BINDIR)

$(AGENT_TARGET): $(BINDIR) $(AGENT_SOURCES)
	$(CC) $(CFLAGS) $(AGENT_SOURCES) -o $(AGENT_TARGET)

clean:
	rm -rf $(BINDIR)

run-agent: $(AGENT_TARGET)
	$(AGENT_TARGET)

.PHONY: all clean run-agent
