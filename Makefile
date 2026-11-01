# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# FLAGS will be passed to both the C and C++ compiler
FLAGS += -Idep/include
CFLAGS +=
CXXFLAGS +=

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS += 
# Add .cpp files to the build
PINK_SOURCES = $(wildcard src/PinkTrombone/*cpp)
SPEECH_SOURCES = $(wildcard src/speech/*.cpp)
SOURCES += $(wildcard src/*.cpp) $(PINK_SOURCES) $(SPEECH_SOURCES)

include third_party/flite/sources.mk
FLITE_OBJECTS := $(patsubst %,build/%.o,$(FLITE_SOURCES))
SOURCES += $(FLITE_SOURCES)
FLAGS += -Ithird_party/flite/include -Ithird_party/flite/lang/usenglish -Ithird_party/flite/lang/cmulex

# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res
DISTRIBUTABLES += third_party/flite/COPYING
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk

.PHONY: test benchmark
test: build/tests/regression
	LD_LIBRARY_PATH="$(RACK_DIR):$$LD_LIBRARY_PATH" $<

benchmark: build/tests/regression
	LD_LIBRARY_PATH="$(RACK_DIR):$$LD_LIBRARY_PATH" $< --benchmark

build/tests/regression: tests/regression.cpp tests/speech.cpp Makefile $(FLITE_OBJECTS) $(wildcard src/*.hpp src/*.cpp src/PinkTrombone/* src/speech/*)
	mkdir -p build/tests
	$(CXX) -iquote src $(CXXFLAGS) tests/regression.cpp $(PINK_SOURCES) $(SPEECH_SOURCES) $(FLITE_OBJECTS) -L$(RACK_DIR) -lRack -pthread -o $@

# Offline English normalization, CMU pronunciation dictionary, and learned
# letter-to-sound fallback. No system Flite library or voice data is needed.

# Keep third-party C warnings/fast-math separate from the module DSP.
$(FLITE_OBJECTS): Makefile third_party/flite/sources.mk
$(FLITE_OBJECTS): CFLAGS := $(filter-out -funsafe-math-optimizations,$(CFLAGS)) -DCST_AUDIO_NONE -DCST_NO_SOCKETS -fvisibility=hidden -w

# Separate objects keep sanitizer builds from contaminating the installed plugin.
SANITIZER_FLAGS = -O1 -fsanitize=address,undefined -fno-omit-frame-pointer
SANITIZER_OBJECTS := $(patsubst %,build/tests/sanitized/%.o,$(FLITE_SOURCES))
build/tests/sanitized/%.c.o: %.c Makefile
	@mkdir -p $(@D)
	$(CC) $(filter-out -O3 -funsafe-math-optimizations,$(CFLAGS)) $(SANITIZER_FLAGS) -DCST_AUDIO_NONE -DCST_NO_SOCKETS -w -c $< -o $@
build/tests/regression-sanitized: tests/regression.cpp tests/speech.cpp Makefile $(SANITIZER_OBJECTS) $(wildcard src/*.hpp src/*.cpp src/PinkTrombone/* src/speech/*)
	@mkdir -p $(@D)
	$(CXX) -iquote src $(filter-out -O3 -funsafe-math-optimizations,$(CXXFLAGS)) $(SANITIZER_FLAGS) tests/regression.cpp $(PINK_SOURCES) $(SPEECH_SOURCES) $(SANITIZER_OBJECTS) -L$(RACK_DIR) -lRack -pthread -o $@
.PHONY: test-sanitized
test-sanitized: build/tests/regression-sanitized
	LD_LIBRARY_PATH="$(RACK_DIR):$$LD_LIBRARY_PATH" $<
