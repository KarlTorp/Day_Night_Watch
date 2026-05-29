#pragma once

#include <TFT_eSPI.h>
#include <LittleFS.h> 

class Label
{
public:

  Label(TFT_eSPI& mTFT, uint16_t x, uint16_t y, uint16_t length, uint16_t txt_size, uint16_t txtColor, uint16_t bgColor = 0xFFFF)
    : mTFT(mTFT), mX(x), mY(y), mWidth(length), mSize(txt_size), mTxtColor(txtColor), mBgColor(bgColor)
  {
  }

  void write(const char* text, bool clear = true)
  {
    int textHeight = 16;
    if (clear)
    {
      if(mSize == 1)
          textHeight = 10;
      mTFT.fillRect(mX, mY, mWidth, textHeight, mBgColor);
    }
    mTFT.setCursor(mX + LABEL_TEXT_MARGIN_X, mY + LABEL_TEXT_MARGIN_Y);
    mTFT.setTextColor(mTxtColor);
    mTFT.setTextSize(mSize);
    mTFT.print(text);
  }

  void write(const char* text, uint16_t color, bool clear = true)
  {
    int textHeight = 16;
    if (clear)
    {
      if(mSize == 1)
          textHeight = 10;
      mTFT.fillRect(mX, mY, mWidth, textHeight, mBgColor);
    }
    mTFT.setCursor(mX + LABEL_TEXT_MARGIN_X, mY + LABEL_TEXT_MARGIN_Y);
    mTFT.setTextColor(mTxtColor);
    mTFT.setTextSize(mSize);
    mTFT.print(text);
  }

  void setTextColor(uint16_t color) { mTxtColor = color; }
  void setBackgroundColor(uint16_t color) { mBgColor = color; }
  void setPosition(uint16_t x, uint16_t y) { mX = x; mY = y; }

  static const uint16_t LABEL_TEXT_MARGIN_X = 0;
  static const uint16_t LABEL_TEXT_MARGIN_Y = 0;

private:
  TFT_eSPI& mTFT;
  uint16_t mX;
  uint16_t mY;
  uint16_t mWidth;
  uint16_t mSize;
  uint16_t mBgColor; 
  uint16_t mTxtColor;
};

class PixelBuffer
{
public:

  PixelBuffer(TFT_eSPI& mTFT, uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t* buffer)
    : mTFT(mTFT), mX(x), mY(y), mWidth(width), mHeight(height), mBuffer(buffer)
    {
    }

  ~PixelBuffer() = default;

  void create(const uint16_t *img, uint16_t width, uint16_t height)
  {
    for(int r=0; r<mHeight; ++r){
        int srcY = mY + r;
        for(int c=0; c<mWidth; ++c){
        int srcX = mX + c;
        uint16_t val = 0;
        if(srcX >= 0 && srcX < width && srcY >= 0 && srcY < height) {
            val = img[srcY * width + srcX];
        }
        mBuffer[r * mWidth + c] = val;
        }
    }
  }

  void draw()
  {
    if (mBuffer) {
      mTFT.pushImage(mX, mY, mWidth, mHeight, mBuffer);
    }
  }

  void setPosition(uint16_t x, uint16_t y) { mX = x; mY = y; }

  // Reads the region covered by this PixelBuffer from a .raw file so draw()
  // can restore the background before the next overlay is rendered.
  void captureFromRaw(const char* path) {
    File f = LittleFS.open(path, "r");
    if (!f) return;
    uint16_t fw, fh;
    f.read((uint8_t*)&fw, 2);
    f.read((uint8_t*)&fh, 2);
    for (uint16_t row = 0; row < mHeight; row++) {
      uint16_t srcY = mY + row;
      if (srcY >= fh) break;
      f.seek(4 + (uint32_t)srcY * fw * 2 + mX * 2);
      f.read((uint8_t*)&mBuffer[row * mWidth], mWidth * 2);
    }
    f.close();
  }

private:
  TFT_eSPI& mTFT;
  uint16_t mX;
  uint16_t mY;
  uint16_t mWidth;
  uint16_t mHeight;
  uint16_t* mBuffer = nullptr;
};

// Converts a 24-bit BMP stored on LittleFS to a compact raw RGB565 file.
// Raw format: 2 bytes width + 2 bytes height, then RGB565 rows top-to-bottom.
// The source BMP is deleted after conversion to free flash space.
inline void convertBmpToRaw(const char* bmpPath, const char* rawPath) {
  File bmp = LittleFS.open(bmpPath, "r");
  if (!bmp) return;

  uint8_t header[54];
  if (bmp.read(header, 54) != 54 || header[0] != 'B' || header[1] != 'M') {
    bmp.close(); return;
  }

  uint32_t dataOffset = header[10] | (header[11]<<8) | (header[12]<<16) | (header[13]<<24);
  int32_t  imgW = header[18] | (header[19]<<8) | (header[20]<<16) | (header[21]<<24);
  int32_t  imgH = header[22] | (header[23]<<8) | (header[24]<<16) | (header[25]<<24);
  uint16_t bpp  = header[28] | (header[29]<<8);

  if (bpp != 24 || imgW < 1 || imgW > 320) { bmp.close(); return; }

  bool flipped = imgH > 0;
  if (imgH < 0) imgH = -imgH;

  File raw = LittleFS.open(rawPath, "w");
  if (!raw) { bmp.close(); return; }

  uint16_t w = (uint16_t)imgW, h = (uint16_t)imgH;
  raw.write((uint8_t*)&w, 2);
  raw.write((uint8_t*)&h, 2);

  uint32_t rowSize = ((imgW * 3 + 3) / 4) * 4;
  uint8_t*  rowBuf = (uint8_t*)malloc(rowSize);
  uint16_t* outBuf = (uint16_t*)malloc(imgW * 2);

  if (rowBuf && outBuf) {
    for (int32_t row = 0; row < imgH; row++) {
      int32_t srcRow = flipped ? (imgH - 1 - row) : row;
      bmp.seek(dataOffset + (uint32_t)srcRow * rowSize);
      bmp.read(rowBuf, rowSize);
      for (int32_t col = 0; col < imgW; col++) {
        uint8_t b = rowBuf[col*3];
        uint8_t g = rowBuf[col*3 + 1];
        uint8_t r = rowBuf[col*3 + 2];
        uint16_t c = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        outBuf[col] = c;
      }
      raw.write((uint8_t*)outBuf, imgW * 2);
    }
  }

  free(rowBuf);
  free(outBuf);
  raw.close();
  bmp.close();
  LittleFS.remove(bmpPath);
  // Serial.printf("Converted %s -> %s (%dx%d)\n", bmpPath, rawPath, imgW, imgH);
}

// Renders a .raw file using pushImage (SPI DMA) — much faster than drawPixel.
inline void drawRaw(TFT_eSPI& tft, const char* path, int x, int y) {
  File f = LittleFS.open(path, "r");
  if (!f) return;

  uint16_t w, h;
  f.read((uint8_t*)&w, 2);
  f.read((uint8_t*)&h, 2);

  uint16_t lineBuf[320];
  for (uint16_t row = 0; row < h; row++) {
    f.read((uint8_t*)lineBuf, w * 2);
    tft.pushImage(x, y + row, w, 1, lineBuf);
  }
  f.close();
}
