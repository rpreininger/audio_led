CXX = g++
CXXFLAGS = -O3 -I../rpi-rgb-led-matrix/include -I./kissfft
LDFLAGS = -L../rpi-rgb-led-matrix/lib
LIBS = -lrgbmatrix -lasound -lpthread

TARGET = audio_led
SOURCES = audio_led.cpp kissfft/kiss_fft.c

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $(TARGET) $(LDFLAGS) $(LIBS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
