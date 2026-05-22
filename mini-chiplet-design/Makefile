CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra -O2 -Iinclude
BLD_DIR  = build

OBJS = $(BLD_DIR)/ucie_d2d.o $(BLD_DIR)/interposer_tech.o \
       $(BLD_DIR)/mcm_integration.o $(BLD_DIR)/d2d_phy.o \
       $(BLD_DIR)/thermal_power.o

export PATH := D:/msys64/mingw64/bin;D:/msys64/usr/bin;$(PATH)
SHELL = cmd.exe

.PHONY: all clean run

all: $(BLD_DIR)/example_d2d_link.exe $(BLD_DIR)/example_interposer.exe \
     $(BLD_DIR)/example_mcm.exe $(BLD_DIR)/demo_ucie.exe \
     $(BLD_DIR)/demo_full_system.exe

$(BLD_DIR):
	@if not exist "$(BLD_DIR)" mkdir "$(BLD_DIR)"

$(BLD_DIR)/ucie_d2d.o: src/ucie_d2d.c | $(BLD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BLD_DIR)/interposer_tech.o: src/interposer_tech.c | $(BLD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BLD_DIR)/mcm_integration.o: src/mcm_integration.c | $(BLD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BLD_DIR)/d2d_phy.o: src/d2d_phy.c | $(BLD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BLD_DIR)/thermal_power.o: src/thermal_power.c | $(BLD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BLD_DIR)/example_d2d_link.exe: examples/example_d2d_link.c $(OBJS)
	$(CC) $(CFLAGS) $^ -lm -o $@

$(BLD_DIR)/example_interposer.exe: examples/example_interposer.c $(OBJS)
	$(CC) $(CFLAGS) $^ -lm -o $@

$(BLD_DIR)/example_mcm.exe: examples/example_mcm.c $(OBJS)
	$(CC) $(CFLAGS) $^ -lm -o $@

$(BLD_DIR)/demo_ucie.exe: demo/demo_ucie.c $(OBJS)
	$(CC) $(CFLAGS) $^ -lm -o $@

$(BLD_DIR)/demo_full_system.exe: demo/demo_full_system.c $(OBJS)
	$(CC) $(CFLAGS) $^ -lm -o $@

run: all
	@echo.
	@echo ===== demo_ucie =====
	$(BLD_DIR)\demo_ucie.exe
	@echo.
	@echo ===== demo_full_system =====
	$(BLD_DIR)\demo_full_system.exe
	@echo.
	@echo ===== example_d2d_link =====
	$(BLD_DIR)\example_d2d_link.exe
	@echo.
	@echo ===== example_interposer =====
	$(BLD_DIR)\example_interposer.exe
	@echo.
	@echo ===== example_mcm =====
	$(BLD_DIR)\example_mcm.exe

clean:
	@if exist "$(BLD_DIR)" rmdir /s /q "$(BLD_DIR)"
