MAKEFLAGS=s
RUNARGS=

projName=bmp-hider
cc=gcc

srcDir=src
targetDir=target
debugDir=$(targetDir)/debug
releaseDir=$(targetDir)/release
selectedDir=$(debugDir)
objDir=$(selectedDir)/obj

debugFlags=-g -Wall -Wextra
releaseFlags=-O2 -DNDEBUG
selectedFlags=$(debugFlags)

srcFiles=$(wildcard $(srcDir)/*.c)
objFiles=$(patsubst $(srcDir)/%.c,$(objDir)/%.o,$(srcFiles))

exe=$(selectedDir)/$(projName).exe

all: debug
release:
	make debug selectedDir='$(releaseDir)' selectedFlags='$(releaseFlags)'
debug: $(exe)

$(exe): $(objFiles)
	$(cc) $(selectedFlags) $(objFiles) -o $(exe)
$(objDir)/%.o: $(srcDir)/%.c | $(objDir)
	$(cc) $(selectedFlags) -c $< -o $@

clean: | $(targetDir)
	rm -r $(targetDir)

run: $(exe)
	./$(exe) $(RUNARGS)

$(targetDir):
	mkdir $@
$(selectedDir): | $(targetDir)
	mkdir $@
$(objDir): | $(selectedDir)
	mkdir $@