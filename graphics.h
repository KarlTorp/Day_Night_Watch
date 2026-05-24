#pragma once

#include <TFT_eSPI.h> 

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

private:
  TFT_eSPI& mTFT;
  uint16_t mX;
  uint16_t mY;
  uint16_t mWidth;
  uint16_t mHeight;
  uint16_t* mBuffer = nullptr;
};
