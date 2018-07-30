// license:BSD-3-Clause
// copyright-holders:Dan Boris
/***************************************************************************

    Atari I, Robot hardware

***************************************************************************/

#include "emu.h"
#include "includes/irobot.h"

#define BITMAP_WIDTH    256


/***************************************************************************

  Convert the color PROMs into a more useable format.

  5 bits from polygon ram address the palette ram

  Output of color RAM
  bit 8 -- inverter -- 1K ohm resistor  -- RED
  bit 7 -- inverter -- 2.2K ohm resistor  -- RED
        -- inverter -- 1K ohm resistor  -- GREEN
        -- inverter -- 2.2K ohm resistor  -- GREEN
        -- inverter -- 1K ohm resistor  -- BLUE
        -- inverter -- 2.2K ohm resistor  -- BLUE
        -- inverter -- 2.2K ohm resistor  -- INT
        -- inverter -- 4.7K ohm resistor  -- INT
  bit 0 -- inverter -- 9.1K ohm resistor  -- INT

  Alphanumeric colors are generated by ROM .125, it's outputs are connected
  to bits 1..8 as above. The inputs are:

  A0..1 - Character color
  A2    - Character image (1=pixel on/0=off)
  A3..4 - Alphamap 0,1 (appears that only Alphamap1 is used, it is set by
          the processor)

***************************************************************************/

PALETTE_INIT_MEMBER(irobot_state, irobot)
{
	const uint8_t *color_prom = memregion("proms")->base();
	int i;

	/* convert the color prom for the text palette */
	for (i = 0; i < 32; i++)
	{
		int intensity = color_prom[i] & 0x03;

		int r = 28 * ((color_prom[i] >> 6) & 0x03) * intensity;
		int g = 28 * ((color_prom[i] >> 4) & 0x03) * intensity;
		int b = 28 * ((color_prom[i] >> 2) & 0x03) * intensity;

		int swapped_i = bitswap<8>(i,7,6,5,4,3,0,1,2);

		palette.set_pen_color(swapped_i + 64, rgb_t(r, g, b));
	}
}


WRITE8_MEMBER(irobot_state::irobot_paletteram_w)
{
	int r,g,b;
	int bits,intensity;
	uint32_t color;

	color = ((data << 1) | (offset & 0x01)) ^ 0x1ff;
	intensity = color & 0x07;
	bits = (color >> 3) & 0x03;
	b = 12 * bits * intensity;
	bits = (color >> 5) & 0x03;
	g = 12 * bits * intensity;
	bits = (color >> 7) & 0x03;
	r = 12 * bits * intensity;
	m_palette->set_pen_color((offset >> 1) & 0x3F,rgb_t(r,g,b));
}


void irobot_state::irobot_poly_clear(uint8_t *bitmap_base)
{
	memset(bitmap_base, 0, BITMAP_WIDTH * m_screen->height());
}

void irobot_state::irobot_poly_clear()
{
	uint8_t *bitmap_base = m_bufsel ? m_polybitmap2.get() : m_polybitmap1.get();
	irobot_poly_clear(bitmap_base);
}


/***************************************************************************

  Start the video hardware emulation.

***************************************************************************/
void irobot_state::video_start()
{
	/* Setup 2 bitmaps for the polygon generator */
	int height = m_screen->height();
	m_polybitmap1 = std::make_unique<uint8_t[]>(BITMAP_WIDTH * height);
	m_polybitmap2 = std::make_unique<uint8_t[]>(BITMAP_WIDTH * height);

	/* clear the bitmaps so we start with valid palette look-up values for drawing */
	irobot_poly_clear(m_polybitmap1.get());
	irobot_poly_clear(m_polybitmap2.get());

	/* Set clipping */
	m_ir_xmin = m_ir_ymin = 0;
	m_ir_xmax = m_screen->width();
	m_ir_ymax = m_screen->height();
}


/***************************************************************************

    Polygon Generator  (Preliminary information)
    The polygon communication ram works as follows (each location is a 16-bit word):

    0000-xxxx: Object pointer table
        bits 00..10: Address of object data
        bits 12..15: Object type
            0x4 = Polygon
            0x8 = Point
            0xC = Vector
        (0xFFFF means end of table)

    Point Object:
        Word 0, bits 0..15: X Position  (0xFFFF = end of point objects)
        Word 1, bits 7..15: Y Position
        bits 0..5: Color

    Vector Object:
        Word 0, bits 7..15: Ending Y   (0xFFFF = end of line objects)
        Word 1, bits 7..15: Starting Y
                bits 0..5: Color
        Word 2: Slope
        Word 3, bits 0..15: Starting X

    Polygon Object:
        Word 0, bits 0..10: Pointer to second slope list
        Word 1, bits 0..15: Starting X first vector
        Word 2, bits 0..15: Starting X second vector
        Word 3, bits 0..5: Color
        bits 7..15: Initial Y value

    Slope Lists: (one starts at Word 4, other starts at pointer in Word 0)
        Word 0, Slope (0xFFFF = side done)
        Word 1, bits 7..15: Ending Y of vector

    Each side is a continuous set of vectors. Both sides are drawn at
    the same time and the space between them is filled in.

***************************************************************************/

#define draw_pixel(x,y,c)       polybitmap[(y) * BITMAP_WIDTH + (x)] = (c)
#define fill_hline(x1,x2,y,c)   memset(&polybitmap[(y) * BITMAP_WIDTH + (x1)], (c), (x2) - (x1) + 1)


/*
     Line draw routine
     modified from a routine written by Andrew Caldwell
 */

void irobot_state::draw_line(uint8_t *polybitmap, int x1, int y1, int x2, int y2, int col)
{
	int dx,dy,sx,sy,cx,cy;

	dx = abs(x1-x2);
	dy = abs(y1-y2);
	sx = (x1 <= x2) ? 1: -1;
	sy = (y1 <= y2) ? 1: -1;
	cx = dx/2;
	cy = dy/2;

	if (dx>=dy)
	{
		for (;;)
		{
			if (x1 >= m_ir_xmin && x1 < m_ir_xmax && y1 >= m_ir_ymin && y1 < m_ir_ymax)
					draw_pixel (x1, y1, col);
				if (x1 == x2) break;
				x1 += sx;
				cx -= dy;
				if (cx < 0)
				{
					y1 += sy;
					cx += dx;
				}
		}
	}
	else
	{
		for (;;)
		{
			if (x1 >= m_ir_xmin && x1 < m_ir_xmax && y1 >= m_ir_ymin && y1 < m_ir_ymax)
				draw_pixel (x1, y1, col);
			if (y1 == y2) break;
			y1 += sy;
			cy -= dx;
			if (cy < 0)
			{
					x1 += sx;
					cy += dy;
			}
		}
	}
}


#define ROUND_TO_PIXEL(x)   ((x >> 7) - 128)

void irobot_state::irobot_run_video()
{
	uint8_t *polybitmap;
	uint16_t *combase16 = (uint16_t *)m_combase;
	int sx,sy,ex,ey,sx2,ey2;
	int color;
	uint32_t d1;
	int lpnt,spnt,spnt2;
	int shp;
	int32_t word1,word2;

	logerror("Starting Polygon Generator, Clear=%d\n",m_vg_clear);

	if (m_bufsel)
		polybitmap = m_polybitmap2.get();
	else
		polybitmap = m_polybitmap1.get();

	lpnt=0;
	while (lpnt < 0x7ff)
	{
		d1 = combase16[lpnt++];
		if (d1 == 0xffff) break;
		spnt = d1 & 0x07ff;
		shp = (d1 & 0xf000) >> 12;

		/* pixel */
		if (shp == 0x8)
		{
			while (spnt < 0x7ff)
			{
				sx = combase16[spnt];
				if (sx == 0xffff) break;
				sy = combase16[spnt+1];
				color = sy & 0x3f;
				sx = ROUND_TO_PIXEL(sx);
				sy = ROUND_TO_PIXEL(sy);
				if (sx >= m_ir_xmin && sx < m_ir_xmax && sy >= m_ir_ymin && sy < m_ir_ymax)
					draw_pixel(sx,sy,color);
				spnt+=2;
			}//while object
		}//if point

		/* line */
		if (shp == 0xc)
		{
			while (spnt < 0x7ff)
			{
				ey = combase16[spnt];
				if (ey == 0xffff) break;
				ey = ROUND_TO_PIXEL(ey);
				sy = combase16[spnt+1];
				color = sy & 0x3f;
				sy = ROUND_TO_PIXEL(sy);
				sx = combase16[spnt+3];
				word1 = (int16_t)combase16[spnt+2];
				ex = sx + word1 * (ey - sy + 1);
				draw_line(polybitmap, ROUND_TO_PIXEL(sx),sy,ROUND_TO_PIXEL(ex),ey,color);
				spnt+=4;
			}//while object
		}//if line

		/* polygon */
		if (shp == 0x4)
		{
			spnt2 = combase16[spnt] & 0x7ff;

			sx = combase16[spnt+1];
			sx2 = combase16[spnt+2];
			sy = combase16[spnt+3];
			color = sy & 0x3f;
			sy = ROUND_TO_PIXEL(sy);
			spnt+=4;

			word1 = (int16_t)combase16[spnt];
			ey = combase16[spnt+1];
			if (word1 != -1 || ey != 0xffff)
			{
				ey = ROUND_TO_PIXEL(ey);
				spnt+=2;

			//  sx += word1;

				word2 = (int16_t)combase16[spnt2];
				ey2 = ROUND_TO_PIXEL(combase16[spnt2+1]);
				spnt2+=2;

			//  sx2 += word2;

				while(1)
				{
					if (sy >= m_ir_ymin && sy < m_ir_ymax)
					{
						int x1 = ROUND_TO_PIXEL(sx);
						int x2 = ROUND_TO_PIXEL(sx2);
						int temp;

						if (x1 > x2) temp = x1, x1 = x2, x2 = temp;
						if (x1 < m_ir_xmin) x1 = m_ir_xmin;
						if (x2 >= m_ir_xmax) x2 = m_ir_xmax - 1;
						if (x1 < x2)
							fill_hline(x1 + 1, x2, sy, color);
					}
					sy++;

					if (sy > ey)
					{
						word1 = (int16_t)combase16[spnt];
						ey = combase16[spnt+1];
						if (word1 == -1 && ey == 0xffff)
							break;
						ey = ROUND_TO_PIXEL(ey);
						spnt+=2;
					}
					else
						sx += word1;

					if (sy > ey2)
					{
						word2 = (int16_t)combase16[spnt2];
						ey2 = ROUND_TO_PIXEL(combase16[spnt2+1]);
						spnt2+=2;
					}
					else
						sx2 += word2;

				} //while polygon
			}//if at least 2 sides
		} //if polygon
	} //while object
}



uint32_t irobot_state::screen_update_irobot(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	uint8_t *videoram = m_videoram;
	uint8_t *bitmap_base = m_bufsel ? m_polybitmap1.get() : m_polybitmap2.get();
	int x, y, offs;

	/* copy the polygon bitmap */
	for (y = cliprect.top(); y <= cliprect.bottom(); y++)
		draw_scanline8(bitmap, 0, y, BITMAP_WIDTH, &bitmap_base[y * BITMAP_WIDTH], nullptr);

	/* redraw the non-zero characters in the alpha layer */
	for (y = offs = 0; y < 32; y++)
		for (x = 0; x < 32; x++, offs++)
		{
			int code = videoram[offs] & 0x3f;
			int color = ((videoram[offs] & 0xc0) >> 6) | (m_alphamap >> 3);

			m_gfxdecode->gfx(0)->transpen(bitmap,cliprect,
					code, color,
					0,0,
					8*x,8*y,0);
		}

	return 0;
}
