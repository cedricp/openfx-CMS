/*
This is the core graphics library for all our displays, providing a common
set of graphics primitives (points, lines, circles, etc.).  It needs to be
paired with a hardware-specific library for each display device we carry
(to handle the lower-level functions).

Adafruit invests time and resources providing this open source code, please
support Adafruit & open-source hardware by purchasing products from Adafruit!

Copyright (c) 2013 Adafruit Industries.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
 */

/*
Adapted for the usage
*/

#include "gfx.h"
#include <stdlib.h>
#include <cstring>
#include <math.h>

#include "font.h"

#ifndef read_byte
#define read_byte(addr) (*(const unsigned char *)(addr))
#endif
#ifndef read_word
#define read_word(addr) (*(const unsigned short *)(addr))
#endif
#ifndef read_dword
#define read_dword(addr) (*(const unsigned long *)(addr))
#endif

inline uint8_t *read_bitmap_ptr(const GFXfont *gfxFont) {
      return gfxFont->bitmap;
}

inline GFXglyph *read_glyph_ptr(const GFXfont *gfxFont, uint8_t c)
{
    return gfxFont->glyph + c;
}

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#define abs(x) ((x) < 0 ? -(x) : (x))

#ifndef _swap_int16_t
#define _swap_int16_t(a, b) \
    {                       \
        int16_t t = a;      \
        a = b;              \
        b = t;              \
    }
#endif

inline float RFLOAT(uint16_t x)
{
    return powf(float((x >> 11 & 0b11111) << 1) / 32, 2.2);
}

inline float GFLOAT(uint16_t x)
{
    return powf(float(x >> 5 & 0b111111) / 32, 2.2);
}

inline float BFLOAT(uint16_t x)
{
    return powf(float((x & 0b11111) << 1) / 32, 2.2);
}

/**************************************************************************/
/*!
   @brief    Instatiate a GFX context for graphics! Can only be done by a
   superclass
   @param    w   Display width, in pixels
   @param    h   Display height, in pixels
*/
/**************************************************************************/
GFX::GFX(int16_t w, int16_t h) : WIDTH(w), HEIGHT(h)
{
    _width = WIDTH;
    _height = HEIGHT;
    cursor_y = cursor_x = 0;
}

/**************************************************************************/
/*!
   @brief    Write a line.  Bresenham's algorithm - thx wikpedia
    @param    x0  Start point x coordinate
    @param    y0  Start point y coordinate
    @param    x1  End point x coordinate
    @param    y1  End point y coordinate
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX::writeLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                    uint16_t color)
{
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep)
    {
        _swap_int16_t(x0, y0);
        _swap_int16_t(x1, y1);
    }

    if (x0 > x1)
    {
        _swap_int16_t(x0, x1);
        _swap_int16_t(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1)
    {
        ystep = 1;
    }
    else
    {
        ystep = -1;
    }

    for (; x0 <= x1; x0++)
    {
        if (steep)
        {
            writePixel(y0, x0, color);
        }
        else
        {
            writePixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0)
        {
            y0 += ystep;
            err += dx;
        }
    }
}

/**************************************************************************/
/*!
   @brief    Start a display-writing routine, overwrite in subclasses.
*/
/**************************************************************************/
void GFX::startWrite() {}

/**************************************************************************/
/*!
   @brief    Write a pixel, overwrite in subclasses if startWrite is defined!
    @param   x   x coordinate
    @param   y   y coordinate
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX::writePixel(int16_t x, int16_t y, uint16_t color)
{
    drawPixel(x, y, color);
}

/**************************************************************************/
/*!
   @brief    Write a perfectly vertical line, overwrite in subclasses if
   startWrite is defined!
    @param    x   Top-most x coordinate
    @param    y   Top-most y coordinate
    @param    h   Height in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX::writeFastVLine(int16_t x, int16_t y, int16_t h,
                         uint16_t color)
{
    // Overwrite in subclasses if startWrite is defined!
    // Can be just writeLine(x, y, x, y+h-1, color);
    // or writeFillRect(x, y, 1, h, color);
    drawFastVLine(x, y, h, color);
}

/**************************************************************************/
/*!
   @brief    Write a perfectly horizontal line, overwrite in subclasses if
   startWrite is defined!
    @param    x   Left-most x coordinate
    @param    y   Left-most y coordinate
    @param    w   Width in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX::writeFastHLine(int16_t x, int16_t y, int16_t w,
                         uint16_t color)
{
    // Overwrite in subclasses if startWrite is defined!
    // Example: writeLine(x, y, x+w-1, y, color);
    // or writeFillRect(x, y, w, 1, color);
    drawFastHLine(x, y, w, color);
}

/**************************************************************************/
/*!
   @brief    Write a rectangle completely with one color, overwrite in
   subclasses if startWrite is defined!
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX::writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                        uint16_t color)
{
    // Overwrite in subclasses if desired!
    fillRect(x, y, w, h, color);
}

/**************************************************************************/
/*!
   @brief    End a display-writing routine, overwrite in subclasses if
   startWrite is defined!
*/
/**************************************************************************/
void GFX::endWrite() {}

/**************************************************************************/
/*!
   @brief    Draw a perfectly vertical line (this is often optimized in a
   subclass!)
    @param    x   Top-most x coordinate
    @param    y   Top-most y coordinate
    @param    h   Height in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX::drawFastVLine(int16_t x, int16_t y, int16_t h,
                        uint16_t color)
{
    startWrite();
    writeLine(x, y, x, y + h - 1, color);
    endWrite();
}

/**************************************************************************/
/*!
   @brief    Draw a perfectly horizontal line (this is often optimized in a
   subclass!)
    @param    x   Left-most x coordinate
    @param    y   Left-most y coordinate
    @param    w   Width in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX::drawFastHLine(int16_t x, int16_t y, int16_t w,
                        uint16_t color)
{
    startWrite();
    writeLine(x, y, x + w - 1, y, color);
    endWrite();
}

/**************************************************************************/
/*!
   @brief    Fill a rectangle completely with one color. Update in subclasses if
   desired!
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
   @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX::fillRect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint16_t color)
{
    startWrite();
    for (int16_t i = x; i < x + w; i++)
    {
        writeFastVLine(i, y, h, color);
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief    Fill the screen completely with one color. Update in subclasses if
   desired!
    @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX::fillScreen(uint16_t color)
{
    fillRect(0, 0, _width, _height, color);
}

/**************************************************************************/
/*!
   @brief    Draw a line
    @param    x0  Start point x coordinate
    @param    y0  Start point y coordinate
    @param    x1  End point x coordinate
    @param    y1  End point y coordinate
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                   uint16_t color)
{
    // Update in subclasses if desired!
    if (x0 == x1)
    {
        if (y0 > y1)
            _swap_int16_t(y0, y1);
        drawFastVLine(x0, y0, y1 - y0 + 1, color);
    }
    else if (y0 == y1)
    {
        if (x0 > x1)
            _swap_int16_t(x0, x1);
        drawFastHLine(x0, y0, x1 - x0 + 1, color);
    }
    else
    {
        startWrite();
        writeLine(x0, y0, x1, y1, color);
        endWrite();
    }
}

/**************************************************************************/
/*!
    @brief Set the font to display when print()ing, either custom or default
    @param  f  The GFXfont object, if NULL use built in 6x8 font
*/
/**************************************************************************/
void GFX::setFont(const GFXfont *f)
{
    if (f)
    { // Font struct pointer passed in?
        if (!gfxFont)
        { // And no current font struct?
            // Switching from classic to new font behavior.
            // Move cursor pos down 6 pixels so it's on baseline.
            cursor_y += 6;
        }
    }
    else if (gfxFont)
    { // NULL passed.  Current font struct defined?
        // Switching from new to classic font behavior.
        // Move cursor pos up 6 pixels so it's at top-left of char.
        cursor_y -= 6;
    }
    gfxFont = (GFXfont *)f;
}

/**************************************************************************/
/*!
    @brief  Helper to determine size of a character with current font/size.
            Broke this out as it's used by both the PROGMEM- and RAM-resident
            getTextBounds() functions.
    @param  c     The ASCII character in question
    @param  x     Pointer to x location of character. Value is modified by
                  this function to advance to next character.
    @param  y     Pointer to y location of character. Value is modified by
                  this function to advance to next character.
    @param  minx  Pointer to minimum X coordinate, passed in to AND returned
                  by this function -- this is used to incrementally build a
                  bounding rectangle for a string.
    @param  miny  Pointer to minimum Y coord, passed in AND returned.
    @param  maxx  Pointer to maximum X coord, passed in AND returned.
    @param  maxy  Pointer to maximum Y coord, passed in AND returned.
*/
/**************************************************************************/
void GFX::charBounds(unsigned char c, int16_t *x, int16_t *y,
                     int16_t *minx, int16_t *miny, int16_t *maxx,
                     int16_t *maxy)
{

    if (gfxFont)
    {

        if (c == '\n')
        {           // Newline?
            *x = 0; // Reset x to zero, advance y by one line
            *y += textsize_y * (uint8_t)read_byte(&gfxFont->yAdvance);
        }
        else if (c != '\r')
        { // Not a carriage return; is normal char
            uint8_t first = read_byte(&gfxFont->first),
                    last = read_byte(&gfxFont->last);
            if ((c >= first) && (c <= last))
            { // Char present in this font?
                GFXglyph *glyph = read_glyph_ptr(gfxFont, c - first);
                uint8_t gw = read_byte(&glyph->width),
                        gh = read_byte(&glyph->height),
                        xa = read_byte(&glyph->xAdvance);
                int8_t xo = read_byte(&glyph->xOffset),
                       yo = read_byte(&glyph->yOffset);
                if (wrap && ((*x + (((int16_t)xo + gw) * textsize_x)) > _width))
                {
                    *x = 0; // Reset x to zero, advance y by one line
                    *y += textsize_y * (uint8_t)read_byte(&gfxFont->yAdvance);
                }
                int16_t tsx = (int16_t)textsize_x, tsy = (int16_t)textsize_y,
                        x1 = *x + xo * tsx, y1 = *y + yo * tsy, x2 = x1 + gw * tsx - 1,
                        y2 = y1 + gh * tsy - 1;
                if (x1 < *minx)
                    *minx = x1;
                if (y1 < *miny)
                    *miny = y1;
                if (x2 > *maxx)
                    *maxx = x2;
                if (y2 > *maxy)
                    *maxy = y2;
                *x += xa * tsx;
            }
        }
    }
    else
    { // Default font

        if (c == '\n')
        {                         // Newline?
            *x = 0;               // Reset x to zero,
            *y += textsize_y * 8; // advance y one line
                                  // min/max x/y unchaged -- that waits for next 'normal' character
        }
        else if (c != '\r')
        { // Normal char; ignore carriage returns
            if (wrap && ((*x + textsize_x * 6) > _width))
            {                         // Off right?
                *x = 0;               // Reset x to zero,
                *y += textsize_y * 8; // advance y one line
            }
            int x2 = *x + textsize_x * 6 - 1, // Lower-right pixel of char
                y2 = *y + textsize_y * 8 - 1;
            if (x2 > *maxx)
                *maxx = x2; // Track max x, y
            if (y2 > *maxy)
                *maxy = y2;
            if (*x < *minx)
                *minx = *x; // Track min x, y
            if (*y < *miny)
                *miny = *y;
            *x += textsize_x * 6; // Advance x one char
        }
    }
}

/**************************************************************************/
/*!
    @brief  Helper to determine size of a string with current font/size.
            Pass string and a cursor position, returns UL corner and W,H.
    @param  str  The ASCII string to measure
    @param  x    The current cursor X
    @param  y    The current cursor Y
    @param  x1   The boundary X coordinate, returned by function
    @param  y1   The boundary Y coordinate, returned by function
    @param  w    The boundary width, returned by function
    @param  h    The boundary height, returned by function
*/
/**************************************************************************/
void GFX::getTextBounds(const char *str, int16_t x, int16_t y,
                        int16_t *x1, int16_t *y1, uint16_t *w,
                        uint16_t *h)
{

    uint8_t c;                                                  // Current character
    int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1; // Bound rect
    // Bound rect is intentionally initialized inverted, so 1st char sets it

    *x1 = x; // Initial position is value passed in
    *y1 = y;
    *w = *h = 0; // Initial size is zero

    while ((c = *str++))
    {
        // charBounds() modifies x/y to advance for each character,
        // and min/max x/y are updated to incrementally build bounding rect.
        charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);
    }

    if (maxx >= minx)
    {                         // If legit string bounds were found...
        *x1 = minx;           // Update x1 to least X coord,
        *w = maxx - minx + 1; // And w to bound rect width
    }
    if (maxy >= miny)
    { // Same for height
        *y1 = miny;
        *h = maxy - miny + 1;
    }
}

void GFX::drawChar(int16_t x, int16_t y, unsigned char c,
                   uint16_t color, uint16_t bg, uint8_t size)
{
    drawChar(x, y, c, color, bg, size, size);
}

void GFX::drawChar(int16_t x, int16_t y, unsigned char c,
                            uint16_t color, uint16_t bg, uint8_t size_x,
                            uint8_t size_y)
{

    if (gfxFont)
    { // Custom font

        // Character is assumed previously filtered by write() to eliminate
        // newlines, returns, non-printable characters, etc.  Calling
        // drawChar() directly with 'bad' characters of font may cause mayhem!

        c -= (uint8_t)read_byte(&gfxFont->first);
        GFXglyph *glyph = read_glyph_ptr(gfxFont, c);
        uint8_t *bitmap = read_bitmap_ptr(gfxFont);

        uint16_t bo = read_word(&glyph->bitmapOffset);
        uint8_t w = read_byte(&glyph->width), h = read_byte(&glyph->height);
        int8_t xo = read_byte(&glyph->xOffset),
               yo = read_byte(&glyph->yOffset);
        uint8_t xx, yy, bits = 0, bit = 0;
        int16_t xo16 = 0, yo16 = 0;

        if (size_x > 1 || size_y > 1)
        {
            xo16 = xo;
            yo16 = yo;
        }

        // Todo: Add character clipping here

        // NOTE: THERE IS NO 'BACKGROUND' COLOR OPTION ON CUSTOM FONTS.
        // THIS IS ON PURPOSE AND BY DESIGN.  The background color feature
        // has typically been used with the 'classic' font to overwrite old
        // screen contents with new data.  This ONLY works because the
        // characters are a uniform size; it's not a sensible thing to do with
        // proportionally-spaced fonts with glyphs of varying sizes (and that
        // may overlap).  To replace previously-drawn text when using a custom
        // font, use the getTextBounds() function to determine the smallest
        // rectangle encompassing a string, erase the area with fillRect(),
        // then draw new text.  This WILL infortunately 'blink' the text, but
        // is unavoidable.  Drawing 'background' pixels will NOT fix this,
        // only creates a new set of problems.  Have an idea to work around
        // this (a canvas object type for MCUs that can afford the RAM and
        // displays supporting setAddrWindow() and pushColors()), but haven't
        // implemented this yet.

        startWrite();
        for (yy = 0; yy < h; yy++)
        {
            for (xx = 0; xx < w; xx++)
            {
                if (!(bit++ & 7))
                {
                    bits = read_byte(&bitmap[bo++]);
                }
                if (bits & 0x80)
                {
                    if (size_x == 1 && size_y == 1)
                    {
                        writePixel(x + xo + xx, y + yo + yy, color);
                    }
                    else
                    {
                        writeFillRect(x + (xo16 + xx) * size_x, y + (yo16 + yy) * size_y,
                                      size_x, size_y, color);
                    }
                }
                bits <<= 1;
            }
        }
        endWrite();

    } // End classic vs custom font
}

/**************************************************************************/
/*!
   @brief    Draw a circle outline
    @param    x0   Center-point x coordinate
    @param    y0   Center-point y coordinate
    @param    r   Radius of circle
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX::drawCircle(int16_t x0, int16_t y0, int16_t r,
                     uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    startWrite();
    writePixel(x0, y0 + r, color);
    writePixel(x0, y0 - r, color);
    writePixel(x0 + r, y0, color);
    writePixel(x0 - r, y0, color);

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        writePixel(x0 + x, y0 + y, color);
        writePixel(x0 - x, y0 + y, color);
        writePixel(x0 + x, y0 - y, color);
        writePixel(x0 - x, y0 - y, color);
        writePixel(x0 + y, y0 + x, color);
        writePixel(x0 - y, y0 + x, color);
        writePixel(x0 + y, y0 - x, color);
        writePixel(x0 - y, y0 - x, color);
    }
    endWrite();
}

/**************************************************************************/
/*!
    @brief    Quarter-circle drawer, used to do circles and roundrects
    @param    x0   Center-point x coordinate
    @param    y0   Center-point y coordinate
    @param    r   Radius of circle
    @param    cornername  Mask bit #1 or bit #2 to indicate which quarters of
   the circle we're doing
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX::drawCircleHelper(int16_t x0, int16_t y0, int16_t r,
                           uint8_t cornername, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        if (cornername & 0x4)
        {
            writePixel(x0 + x, y0 + y, color);
            writePixel(x0 + y, y0 + x, color);
        }
        if (cornername & 0x2)
        {
            writePixel(x0 + x, y0 - y, color);
            writePixel(x0 + y, y0 - x, color);
        }
        if (cornername & 0x8)
        {
            writePixel(x0 - y, y0 + x, color);
            writePixel(x0 - x, y0 + y, color);
        }
        if (cornername & 0x1)
        {
            writePixel(x0 - y, y0 - x, color);
            writePixel(x0 - x, y0 - y, color);
        }
    }
}

/**************************************************************************/
/*!
   @brief    Draw a circle with filled color
    @param    x0   Center-point x coordinate
    @param    y0   Center-point y coordinate
    @param    r   Radius of circle
    @param    color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX::fillCircle(int16_t x0, int16_t y0, int16_t r,
                     uint16_t color)
{
    startWrite();
    writeFastVLine(x0, y0 - r, 2 * r + 1, color);
    fillCircleHelper(x0, y0, r, 3, 0, color);
    endWrite();
}

/**************************************************************************/
/*!
    @brief  Quarter-circle drawer with fill, used for circles and roundrects
    @param  x0       Center-point x coordinate
    @param  y0       Center-point y coordinate
    @param  r        Radius of circle
    @param  corners  Mask bits indicating which quarters we're doing
    @param  delta    Offset from center-point, used for round-rects
    @param  color    16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFX::fillCircleHelper(int16_t x0, int16_t y0, int16_t r,
                           uint8_t corners, int16_t delta,
                           uint16_t color)
{

    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;
    int16_t px = x;
    int16_t py = y;

    delta++; // Avoid some +1's in the loop

    while (x < y)
    {
        if (f >= 0)
        {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        // These checks avoid double-drawing certain lines, important
        // for the SSD1306 library which has an INVERT drawing mode.
        if (x < (y + 1))
        {
            if (corners & 1)
                writeFastVLine(x0 + x, y0 - y, 2 * y + delta, color);
            if (corners & 2)
                writeFastVLine(x0 - x, y0 - y, 2 * y + delta, color);
        }
        if (y != py)
        {
            if (corners & 1)
                writeFastVLine(x0 + py, y0 - px, 2 * px + delta, color);
            if (corners & 2)
                writeFastVLine(x0 - py, y0 - px, 2 * px + delta, color);
            py = y;
        }
        px = x;
    }
}

/**************************************************************************/
/*!
   @brief   Draw a rectangle with no fill color
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX::drawRect(int16_t x, int16_t y, int16_t w, int16_t h,
                   uint16_t color)
{
    startWrite();
    writeFastHLine(x, y, w, color);
    writeFastHLine(x, y + h - 1, w, color);
    writeFastVLine(x, y, h, color);
    writeFastVLine(x + w - 1, y, h, color);
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a rounded rectangle with no fill color
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
    @param    r   Radius of corner rounding
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                        int16_t r, uint16_t color)
{
    int16_t max_radius = ((w < h) ? w : h) / 2; // 1/2 minor axis
    if (r > max_radius)
        r = max_radius;
    // smarter version
    startWrite();
    writeFastHLine(x + r, y, w - 2 * r, color);         // Top
    writeFastHLine(x + r, y + h - 1, w - 2 * r, color); // Bottom
    writeFastVLine(x, y + r, h - 2 * r, color);         // Left
    writeFastVLine(x + w - 1, y + r, h - 2 * r, color); // Right
    // draw four corners
    drawCircleHelper(x + r, y + r, r, 1, color);
    drawCircleHelper(x + w - r - 1, y + r, r, 2, color);
    drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
    drawCircleHelper(x + r, y + h - r - 1, r, 8, color);
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a rounded rectangle with fill color
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    w   Width in pixels
    @param    h   Height in pixels
    @param    r   Radius of corner rounding
    @param    color 16-bit 5-6-5 Color to draw/fill with
*/
/**************************************************************************/
void GFX::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h,
                        int16_t r, uint16_t color)
{
    int16_t max_radius = ((w < h) ? w : h) / 2; // 1/2 minor axis
    if (r > max_radius)
        r = max_radius;
    // smarter version
    startWrite();
    writeFillRect(x + r, y, w - 2 * r, h, color);
    // draw four corners
    fillCircleHelper(x + w - r - 1, y + r, r, 1, h - 2 * r - 1, color);
    fillCircleHelper(x + r, y + r, r, 2, h - 2 * r - 1, color);
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a triangle with no fill color
    @param    x0  Vertex #0 x coordinate
    @param    y0  Vertex #0 y coordinate
    @param    x1  Vertex #1 x coordinate
    @param    y1  Vertex #1 y coordinate
    @param    x2  Vertex #2 x coordinate
    @param    y2  Vertex #2 y coordinate
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color)
{
    drawLine(x0, y0, x1, y1, color);
    drawLine(x1, y1, x2, y2, color);
    drawLine(x2, y2, x0, y0, color);
}

/**************************************************************************/
/*!
   @brief     Draw a triangle with color-fill
    @param    x0  Vertex #0 x coordinate
    @param    y0  Vertex #0 y coordinate
    @param    x1  Vertex #1 x coordinate
    @param    y1  Vertex #1 y coordinate
    @param    x2  Vertex #2 x coordinate
    @param    y2  Vertex #2 y coordinate
    @param    color 16-bit 5-6-5 Color to fill/draw with
*/
/**************************************************************************/
void GFX::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                       int16_t x2, int16_t y2, uint16_t color)
{

    int16_t a, b, y, last;

    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if (y0 > y1)
    {
        _swap_int16_t(y0, y1);
        _swap_int16_t(x0, x1);
    }
    if (y1 > y2)
    {
        _swap_int16_t(y2, y1);
        _swap_int16_t(x2, x1);
    }
    if (y0 > y1)
    {
        _swap_int16_t(y0, y1);
        _swap_int16_t(x0, x1);
    }

    startWrite();
    if (y0 == y2)
    { // Handle awkward all-on-same-line case as its own thing
        a = b = x0;
        if (x1 < a)
            a = x1;
        else if (x1 > b)
            b = x1;
        if (x2 < a)
            a = x2;
        else if (x2 > b)
            b = x2;
        writeFastHLine(a, y0, b - a + 1, color);
        endWrite();
        return;
    }

    int16_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
            dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0, sb = 0;

    // For upper part of triangle, find scanline crossings for segments
    // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
    // is included here (and second loop will be skipped, avoiding a /0
    // error there), otherwise scanline y1 is skipped here and handled
    // in the second loop...which also avoids a /0 error here if y0=y1
    // (flat-topped triangle).
    if (y1 == y2)
        last = y1; // Include y1 scanline
    else
        last = y1 - 1; // Skip it

    for (y = y0; y <= last; y++)
    {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;
        /* longhand:
        a = x0 + (x1 - x0) * (y - y0) / (y1 - y0);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if (a > b)
            _swap_int16_t(a, b);
        writeFastHLine(a, y, b - a + 1, color);
    }

    // For lower part of triangle, find scanline crossings for segments
    // 0-2 and 1-2.  This loop is skipped if y1=y2.
    sa = (int32_t)dx12 * (y - y1);
    sb = (int32_t)dx02 * (y - y0);
    for (; y <= y2; y++)
    {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;
        /* longhand:
        a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
        b = x0 + (x2 - x0) * (y - y0) / (y2 - y0);
        */
        if (a > b)
            _swap_int16_t(a, b);
        writeFastHLine(a, y, b - a + 1, color);
    }
    endWrite();
}

// BITMAP / XBITMAP / GRAYSCALE / RGB BITMAP FUNCTIONS ---------------------

/**************************************************************************/
/*!
   @brief      Draw a PROGMEM-resident 1-bit image at the specified (x,y)
   position, using the specified foreground color (unset bits are transparent).
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
                     int16_t w, int16_t h, uint16_t color)
{

    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t b = 0;

    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                b <<= 1;
            else
                b = read_byte(&bitmap[j * byteWidth + i / 8]);
            if (b & 0x80)
                writePixel(x + i, y, color);
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief      Draw a PROGMEM-resident 1-bit image at the specified (x,y)
   position, using the specified foreground (for set bits) and background (unset
   bits) colors.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 16-bit 5-6-5 Color to draw pixels with
    @param    bg 16-bit 5-6-5 Color to draw background with
*/
/**************************************************************************/
void GFX::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
                     int16_t w, int16_t h, uint16_t color,
                     uint16_t bg)
{

    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t b = 0;

    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                b <<= 1;
            else
                b = read_byte(&bitmap[j * byteWidth + i / 8]);
            writePixel(x + i, y, (b & 0x80) ? color : bg);
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief      Draw a RAM-resident 1-bit image at the specified (x,y) position,
   using the specified foreground color (unset bits are transparent).
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 16-bit 5-6-5 Color to draw with
*/
/**************************************************************************/
void GFX::drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w,
                     int16_t h, uint16_t color)
{

    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t b = 0;

    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                b <<= 1;
            else
                b = bitmap[j * byteWidth + i / 8];
            if (b & 0x80)
                writePixel(x + i, y, color);
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief      Draw a RAM-resident 1-bit image at the specified (x,y) position,
   using the specified foreground (for set bits) and background (unset bits)
   colors.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 16-bit 5-6-5 Color to draw pixels with
    @param    bg 16-bit 5-6-5 Color to draw background with
*/
/**************************************************************************/
void GFX::drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w,
                     int16_t h, uint16_t color, uint16_t bg)
{

    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t b = 0;

    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                b <<= 1;
            else
                b = bitmap[j * byteWidth + i / 8];
            writePixel(x + i, y, (b & 0x80) ? color : bg);
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief      Draw PROGMEM-resident XBitMap Files (*.xbm), exported from GIMP.
   Usage: Export from GIMP to *.xbm, rename *.xbm to *.c and open in editor.
   C Array can be directly used with this function.
   There is no RAM-resident version of this function; if generating bitmaps
   in RAM, use the format defined by drawBitmap() and call that instead.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with monochrome bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
    @param    color 16-bit 5-6-5 Color to draw pixels with
*/
/**************************************************************************/
void GFX::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[],
                      int16_t w, int16_t h, uint16_t color)
{

    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t b = 0;

    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                b >>= 1;
            else
                b = read_byte(&bitmap[j * byteWidth + i / 8]);
            // Nearly identical to drawBitmap(), only the bit order
            // is reversed here (left-to-right = LSB to MSB):
            if (b & 0x01)
                writePixel(x + i, y, color);
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a PROGMEM-resident 8-bit image (grayscale) at the specified
   (x,y) pos. Specifically for 8-bit display devices such as IS31FL3731; no
   color reduction/expansion is performed.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with grayscale bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
*/
/**************************************************************************/
void GFX::drawGrayscaleBitmap(int16_t x, int16_t y,
                              const uint8_t bitmap[], int16_t w,
                              int16_t h)
{
    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            writePixel(x + i, y, (uint8_t)read_byte(&bitmap[j * w + i]));
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a RAM-resident 8-bit image (grayscale) at the specified (x,y)
   pos. Specifically for 8-bit display devices such as IS31FL3731; no color
   reduction/expansion is performed.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with grayscale bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
*/
/**************************************************************************/
void GFX::drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap,
                              int16_t w, int16_t h)
{
    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            writePixel(x + i, y, bitmap[j * w + i]);
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a PROGMEM-resident 8-bit image (grayscale) with a 1-bit mask
   (set bits = opaque, unset bits = clear) at the specified (x,y) position.
   BOTH buffers (grayscale and mask) must be PROGMEM-resident.
   Specifically for 8-bit display devices such as IS31FL3731; no color
   reduction/expansion is performed.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with grayscale bitmap
    @param    mask  byte array with mask bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
*/
/**************************************************************************/
void GFX::drawGrayscaleBitmap(int16_t x, int16_t y,
                              const uint8_t bitmap[],
                              const uint8_t mask[], int16_t w,
                              int16_t h)
{
    int16_t bw = (w + 7) / 8; // Bitmask scanline pad = whole byte
    uint8_t b = 0;
    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                b <<= 1;
            else
                b = read_byte(&mask[j * bw + i / 8]);
            if (b & 0x80)
            {
                writePixel(x + i, y, (uint8_t)read_byte(&bitmap[j * w + i]));
            }
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a RAM-resident 8-bit image (grayscale) with a 1-bit mask
   (set bits = opaque, unset bits = clear) at the specified (x,y) position.
   BOTH buffers (grayscale and mask) must be RAM-residentt, no mix-and-match
   Specifically for 8-bit display devices such as IS31FL3731; no color
   reduction/expansion is performed.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with grayscale bitmap
    @param    mask  byte array with mask bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
*/
/**************************************************************************/
void GFX::drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap,
                              uint8_t *mask, int16_t w, int16_t h)
{
    int16_t bw = (w + 7) / 8; // Bitmask scanline pad = whole byte
    uint8_t b = 0;
    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                b <<= 1;
            else
                b = mask[j * bw + i / 8];
            if (b & 0x80)
            {
                writePixel(x + i, y, bitmap[j * w + i]);
            }
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a PROGMEM-resident 16-bit image (RGB 5/6/5) at the specified
   (x,y) position. For 16-bit display devices; no color reduction performed.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with 16-bit color bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
*/
/**************************************************************************/
void GFX::drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[],
                        int16_t w, int16_t h)
{
    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            writePixel(x + i, y, read_word(&bitmap[j * w + i]));
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a RAM-resident 16-bit image (RGB 5/6/5) at the specified (x,y)
   position. For 16-bit display devices; no color reduction performed.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with 16-bit color bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
*/
/**************************************************************************/
void GFX::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap,
                        int16_t w, int16_t h)
{
    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            writePixel(x + i, y, bitmap[j * w + i]);
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a PROGMEM-resident 16-bit image (RGB 5/6/5) with a 1-bit mask
   (set bits = opaque, unset bits = clear) at the specified (x,y) position. BOTH
   buffers (color and mask) must be PROGMEM-resident. For 16-bit display
   devices; no color reduction performed.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with 16-bit color bitmap
    @param    mask  byte array with monochrome mask bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
*/
/**************************************************************************/
void GFX::drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[],
                        const uint8_t mask[], int16_t w, int16_t h)
{
    int16_t bw = (w + 7) / 8; // Bitmask scanline pad = whole byte
    uint8_t b = 0;
    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                b <<= 1;
            else
                b = read_byte(&mask[j * bw + i / 8]);
            if (b & 0x80)
            {
                writePixel(x + i, y, read_word(&bitmap[j * w + i]));
            }
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
   @brief   Draw a RAM-resident 16-bit image (RGB 5/6/5) with a 1-bit mask (set
   bits = opaque, unset bits = clear) at the specified (x,y) position. BOTH
   buffers (color and mask) must be RAM-resident. For 16-bit display devices; no
   color reduction performed.
    @param    x   Top left corner x coordinate
    @param    y   Top left corner y coordinate
    @param    bitmap  byte array with 16-bit color bitmap
    @param    mask  byte array with monochrome mask bitmap
    @param    w   Width of bitmap in pixels
    @param    h   Height of bitmap in pixels
*/
/**************************************************************************/
void GFX::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap,
                        uint8_t *mask, int16_t w, int16_t h)
{
    int16_t bw = (w + 7) / 8; // Bitmask scanline pad = whole byte
    uint8_t b = 0;
    startWrite();
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            if (i & 7)
                b <<= 1;
            else
                b = mask[j * bw + i / 8];
            if (b & 0x80)
            {
                writePixel(x + i, y, bitmap[j * w + i]);
            }
        }
    }
    endWrite();
}

/**************************************************************************/
/*!
    @brief      Invert the display (ideally using built-in hardware command)
    @param   i  True if you want to invert, false to make 'normal'
*/
/**************************************************************************/
void GFX::invertDisplay(bool i)
{
    // Do nothing, must be subclassed if supported by hardware
    (void)i; // disable -Wunused-parameter warning
}

/**************************************************************************/
/*!
   @brief    Instatiate a GFX 16-bit canvas context for graphics
   @param    w   Display width, in pixels
   @param    h   Display height, in pixels
   @param    allocate_buffer If true, a buffer is allocated with malloc. If
   false, the subclass must initialize the buffer before any drawing operation,
   and free it in the destructor. If false (the default), the buffer is
   allocated and freed by the library.
*/
/**************************************************************************/
GFXcanvasFloat::GFXcanvasFloat(uint16_t w, uint16_t h, float *buffer_in)
    : GFX(w, h)
{
    rotation = 4;
    uint32_t bytes = w * 3 * h * sizeof(float);
    buffer = buffer_in;
    memset(buffer, 0, bytes);
    setFont(&FreeSans12pt7b);
}

/**************************************************************************/
/*!
   @brief    Delete the canvas, free memory
*/
/**************************************************************************/
GFXcanvasFloat::~GFXcanvasFloat(void)
{
}

/**************************************************************************/
/*!
    @brief  Draw a pixel to the canvas framebuffer
    @param  x   x coordinate
    @param  y   y coordinate
    @param  color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFXcanvasFloat::drawPixel(int16_t x, int16_t y, uint16_t color)
{
    if (buffer)
    {
        if ((x < 0) || (y < 0) || (x >= _width) || (y >= _height))
            return;

        int16_t t;
        switch (rotation)
        {
        case 1:
            t = x;
            x = WIDTH - 1 - y;
            y = t;
            break;
        case 2:
            x = WIDTH - 1 - x;
            y = HEIGHT - 1 - y;
            break;
        case 3:
            t = x;
            x = y;
            y = HEIGHT - 1 - t;
            break;
        case 4:
            y = HEIGHT - 1 - y;
            break;
        }
        size_t index = (x * 3) + (y * WIDTH * 3);
        buffer[index] = RFLOAT(color);
        buffer[index + 1] = GFLOAT(color);
        buffer[index + 2] = BFLOAT(color);
    }
}

/**************************************************************************/
/*!
    @brief  Fill the framebuffer completely with one color
    @param  color 16-bit 5-6-5 Color to fill with
*/
/**************************************************************************/
void GFXcanvasFloat::fillScreen(uint16_t color)
{
    if (buffer)
    {
        uint32_t i, pixels = WIDTH * HEIGHT;
        for (i = 0; i < pixels; i += 3)
        {
            buffer[i] = RFLOAT(color);
            buffer[i + 1] = GFLOAT(color);
            buffer[i + 2] = BFLOAT(color);
        }
    }
}

/**************************************************************************/
/*!
   @brief    Speed optimized vertical line drawing
   @param    x   Line horizontal start point
   @param    y   Line vertical start point
   @param    h   length of vertical line to be drawn, including first point
   @param    color   color 16-bit 5-6-5 Color to draw line with
*/
/**************************************************************************/
void GFXcanvasFloat::drawFastVLine(int16_t x, int16_t y, int16_t h,
                                   uint16_t color)
{
    if (h < 0)
    { // Convert negative heights to positive equivalent
        h *= -1;
        y -= h - 1;
        if (y < 0)
        {
            h += y;
            y = 0;
        }
    }

    // Edge rejection (no-draw if totally off canvas)
    if ((x < 0) || (x >= width()) || (y >= height()) || ((y + h - 1) < 0))
    {
        return;
    }

    if (y < 0)
    { // Clip top
        h += y;
        y = 0;
    }
    if (y + h > height())
    { // Clip bottom
        h = height() - y;
    }

    if (getRotation() == 0)
    {
        drawFastRawVLine(x, y, h, color);
    }
    else if (getRotation() == 1)
    {
        int16_t t = x;
        x = WIDTH - 1 - y;
        y = t;
        x -= h - 1;
        drawFastRawHLine(x, y, h, color);
    }
    else if (getRotation() == 2)
    {
        x = WIDTH - 1 - x;
        y = HEIGHT - 1 - y;

        y -= h - 1;
        drawFastRawVLine(x, y, h, color);
    }
    else if (getRotation() == 3)
    {
        int16_t t = x;
        x = y;
        y = HEIGHT - 1 - t;
        drawFastRawHLine(x, y, h, color);
    }
    else if (getRotation() == 4)
    {
        y = HEIGHT - 1 - y;
        y -= h - 1;
        drawFastRawVLine(x, y, h, color);
    }
}

/**************************************************************************/
/*!
   @brief  Speed optimized horizontal line drawing
   @param  x      Line horizontal start point
   @param  y      Line vertical start point
   @param  w      Length of horizontal line to be drawn, including 1st point
   @param  color  Color 16-bit 5-6-5 Color to draw line with
*/
/**************************************************************************/
void GFXcanvasFloat::drawFastHLine(int16_t x, int16_t y, int16_t w,
                                   uint16_t color)
{
    if (w < 0)
    { // Convert negative widths to positive equivalent
        w *= -1;
        x -= w - 1;
        if (x < 0)
        {
            w += x;
            x = 0;
        }
    }

    // Edge rejection (no-draw if totally off canvas)
    if ((y < 0) || (y >= height()) || (x >= width()) || ((x + w - 1) < 0))
    {
        return;
    }

    if (x < 0)
    { // Clip left
        w += x;
        x = 0;
    }
    if (x + w >= width())
    { // Clip right
        w = width() - x;
    }

    if (getRotation() == 0)
    {
        drawFastRawHLine(x, y, w, color);
    }
    else if (getRotation() == 1)
    {
        int16_t t = x;
        x = WIDTH - 1 - y;
        y = t;
        drawFastRawVLine(x, y, w, color);
    }
    else if (getRotation() == 2)
    {
        x = WIDTH - 1 - x;
        y = HEIGHT - 1 - y;

        x -= w - 1;
        drawFastRawHLine(x, y, w, color);
    }
    else if (getRotation() == 3)
    {
        int16_t t = x;
        x = y;
        y = HEIGHT - 1 - t;
        y -= w - 1;
        drawFastRawVLine(x, y, w, color);
    }
    else if (getRotation() == 4)
    {
        y = HEIGHT - 1 - y;
        drawFastRawHLine(x, y, w, color);
    }
}

/**************************************************************************/
/*!
   @brief    Speed optimized vertical line drawing into the raw canvas buffer
   @param    x   Line horizontal start point
   @param    y   Line vertical start point
   @param    h   length of vertical line to be drawn, including first point
   @param    color   color 16-bit 5-6-5 Color to draw line with
*/
/**************************************************************************/
void GFXcanvasFloat::drawFastRawVLine(int16_t x, int16_t y, int16_t h,
                                      uint16_t color)
{
    // x & y already in raw (rotation 0) coordinates, no need to transform.
    float *buffer_ptr = buffer + y * WIDTH * 3 + x * 3;
    for (int16_t i = 0; i < h; i++)
    {
        buffer_ptr[0] = RFLOAT(color);
        buffer_ptr[1] = GFLOAT(color);
        buffer_ptr[2] = BFLOAT(color);
        buffer_ptr += WIDTH * 3;
    }
}

/**************************************************************************/
/*!
   @brief    Speed optimized horizontal line drawing into the raw canvas buffer
   @param    x   Line horizontal start point
   @param    y   Line vertical start point
   @param    w   length of horizontal line to be drawn, including first point
   @param    color   color 16-bit 5-6-5 Color to draw line with
*/
/**************************************************************************/
void GFXcanvasFloat::drawFastRawHLine(int16_t x, int16_t y, int16_t w,
                                      uint16_t color)
{
    // x & y already in raw (rotation 0) coordinates, no need to transform.
    uint32_t buffer_index = y * WIDTH * 3 + x * 3;
    for (uint32_t i = buffer_index; i < buffer_index + w * 3; i += 3)
    {
        buffer[i] = RFLOAT(color);
        buffer[i + 1] = GFLOAT(color);
        buffer[i + 2] = BFLOAT(color);
    }
}