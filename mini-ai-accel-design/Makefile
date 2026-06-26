CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c99 -I include
LDFLAGS = -lm

SRCDIR = src
TSTDIR = tests
EXDIR = examples
DEMDIR = demos
OUTDIR = bin

SRCS = $(SRCDIR)/dnn_isa.c $(SRCDIR)/pe_microarch.c $(SRCDIR)/systolic_array.c \
       $(SRCDIR)/buffer_hierarchy.c $(SRCDIR)/accel_roofline.c
OBJS = $(SRCS:$(SRCDIR)/%.c=$(OUTDIR)/%.o)

.PHONY: all test clean examples demos

all: $(OUTDIR) $(OUTDIR)/test_accel.exe $(OUTDIR)/example_mnist.exe \
     $(OUTDIR)/example_resnet.exe $(OUTDIR)/example_bert.exe $(OUTDIR)/demo_benchmark.exe

$(OUTDIR):
	-mkdir $(OUTDIR)

$(OUTDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OUTDIR)/test_accel.exe: $(OBJS) $(TSTDIR)/test_accel.c
	$(CC) $(CFLAGS) -o $@ $(TSTDIR)/test_accel.c $(OBJS) $(LDFLAGS)

$(OUTDIR)/example_mnist.exe: $(OBJS) $(EXDIR)/example_mnist.c
	$(CC) $(CFLAGS) -o $@ $(EXDIR)/example_mnist.c $(OBJS) $(LDFLAGS)

$(OUTDIR)/example_resnet.exe: $(OBJS) $(EXDIR)/example_resnet.c
	$(CC) $(CFLAGS) -o $@ $(EXDIR)/example_resnet.c $(OBJS) $(LDFLAGS)

$(OUTDIR)/example_bert.exe: $(OBJS) $(EXDIR)/example_bert.c
	$(CC) $(CFLAGS) -o $@ $(EXDIR)/example_bert.c $(OBJS) $(LDFLAGS)

$(OUTDIR)/demo_benchmark.exe: $(OBJS) $(DEMDIR)/demo_benchmark.c
	$(CC) $(CFLAGS) -o $@ $(DEMDIR)/demo_benchmark.c $(OBJS) $(LDFLAGS)

test: $(OUTDIR)/test_accel.exe
	$(OUTDIR)/test_accel.exe

examples: $(OUTDIR)/example_mnist.exe $(OUTDIR)/example_resnet.exe $(OUTDIR)/example_bert.exe
	@echo === MNIST Example ===
	$(OUTDIR)/example_mnist.exe
	@echo.
	@echo === ResNet Example ===
	$(OUTDIR)/example_resnet.exe
	@echo.
	@echo === BERT Example ===
	$(OUTDIR)/example_bert.exe

demos: $(OUTDIR)/demo_benchmark.exe
	$(OUTDIR)/demo_benchmark.exe

clean:
	-del /Q $(OUTDIR)\*.o 2>nul
	-del /Q $(OUTDIR)\*.exe 2>nul
	-rmdir /S /Q $(OUTDIR) 2>nul
