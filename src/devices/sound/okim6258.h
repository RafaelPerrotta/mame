// license:BSD-3-Clause
// copyright-holders:Barry Rodewald
#ifndef MAME_SOUND_OKIM6258_H
#define MAME_SOUND_OKIM6258_H

#pragma once


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> okim6258_device

class okim6258_device : public device_t,
						public device_sound_interface
{
public:
	static constexpr int FOSC_DIV_BY_1024    = 0;
	static constexpr int FOSC_DIV_BY_768     = 1;
	static constexpr int FOSC_DIV_BY_512     = 2;

	static constexpr int TYPE_3BITS          = 0;
	static constexpr int TYPE_4BITS          = 1;

	static constexpr int OUTPUT_10BITS       = 10;
	static constexpr int OUTPUT_12BITS       = 12;

	okim6258_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// configuration
	void set_start_div(int div) { m_start_divider = div; }
	void set_type(int type) { m_adpcm_type = type; }
	void set_outbits(int outbit) { m_output_bits = outbit; }

	DECLARE_READ8_MEMBER( status_r );
	DECLARE_WRITE8_MEMBER( data_w );
	DECLARE_WRITE8_MEMBER( ctrl_w );

	void set_divider(int val);
	int get_vclk();

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_clock_changed() override;

	// sound stream update overrides
	virtual void sound_stream_update(sound_stream &stream, stream_sample_t **inputs, stream_sample_t **outputs, int samples) override;

private:
	void state_save_register();
	int16_t clock_adpcm(uint8_t nibble);

	uint8_t  m_status;

	uint32_t m_start_divider;
	uint32_t m_divider;         /* master clock divider */
	uint8_t m_adpcm_type;       /* 3/4 bit ADPCM select */
	uint8_t m_data_in;          /* ADPCM data-in register */
	uint8_t m_nibble_shift;     /* nibble select */
	sound_stream *m_stream;   /* which stream are we playing on? */

	uint8_t m_output_bits;      /* D/A precision is 10-bits but 12-bit data can be
	                           output serially to an external DAC */

	int32_t m_signal;
	int32_t m_step;
};

DECLARE_DEVICE_TYPE(OKIM6258, okim6258_device)

#endif // MAME_SOUND_OKIM6258_H
