#include "pch.h"

#include "Utilities/Serializer.h"

#include "NES/NesPpu.h"
#include "NES/NesCpu.h"
#include "NES/APU/NesApu.h"
#include "NES/NesMemoryManager.h"
#include "NES/NesConsole.h"
#include "NES/NesControlManager.h"
#include "NES/BaseMapper.h"
#include "NES/NesConstants.h"
#include "NES/NesDefaultVideoFilter.h"

#include "NES/DefaultNesPpu.h"
#include "NES/NsfPpu.h"
#include "NES/HdPacks/HdNesPpu.h"
#include "NES/HdPacks/HdBuilderPpu.h"

#include "Shared/EmuSettings.h"
#include "Shared/Video/VideoDecoder.h"
#include "Shared/RewindManager.h"
#include "Shared/NotificationManager.h"
#include "Shared/RenderedFrame.h"
#include "Shared/MemoryOperationType.h"
#include "Shared/Utilities/AvMergeUtilities.h"

#include "Shared/EventType.h"

#include "Utilities/BitUtilities.h"

template<class T> NesPpu<T>::NesPpu(NesConsole* console)
{
	_console = console;
	_emu = console->GetEmulator();
	_mapper = console->GetMapper();
	_masterClock = 0;
	_masterClockDivider = 4;
	_settings = _emu->GetSettings();

	_outputBuffers[0] = new uint16_t[256 * 240];
	_outputBuffers[1] = new uint16_t[256 * 240];

	_currentOutputBuffer = _outputBuffers[0];
	memset(_outputBuffers[0], 0, 256 * 240 * sizeof(uint16_t));
	memset(_outputBuffers[1], 0, 256 * 240 * sizeof(uint16_t));

	if(_emu->GetSettings()->GetNesConfig().RamPowerOnState == RamState::Random) {
		_console->InitializeRam(_paletteRam, 0x20);
		for(int i = 0; i < 0x20; i++) {
			_paletteRam[i] &= 0x3F;
		}
	} else {
		//When not using random ram, use a static state at power on (matches blargg's old palette test rom)
		constexpr uint8_t paletteRamBootValues[0x20] {
			0x09, 0x01, 0x00, 0x01, 0x00, 0x02, 0x02, 0x0D, 0x08, 0x10, 0x08, 0x24, 0x00, 0x00, 0x04, 0x2C, 0x09, 0x01, 0x34, 0x03, 0x00, 0x04, 0x00, 0x14, 0x08, 0x3A, 0x00, 0x02, 0x00, 0x20, 0x2C, 0x08
		};
		memcpy(_paletteRam, paletteRamBootValues, sizeof(_paletteRam));
	}

	//'v' is not cleared on reset, but it set to 0 on power on
	_videoRamAddr = 0;

	_emu->RegisterMemory(MemoryType::NesSpriteRam, _spriteRam, sizeof(_spriteRam));
	_emu->RegisterMemory(MemoryType::NesSecondarySpriteRam, _secondarySpriteRam, sizeof(_secondarySpriteRam));
	_emu->RegisterMemory(MemoryType::NesPaletteRam, _paletteRam, sizeof(_paletteRam));

	_console->InitializeRam(_spriteRam, 0x100);
	_console->InitializeRam(_secondarySpriteRam, 0x20);

	UpdateTimings(ConsoleRegion::Ntsc);

	Reset(false);
}

template<class T> void NesPpu<T>::Reset(bool softReset)
{
	_masterClock = 0;

	//Reset OAM decay timestamps regardless of the reset PPU option
	memset(_oamDecayCycles, 0, sizeof(_oamDecayCycles));
	_enableOamDecay = _console->GetNesConfig().EnableOamDecay;

	if(softReset && _settings->GetNesConfig().DisablePpuReset) {
		return;
	}

	_preventVblFlag = false;

	_needStateUpdate = false;
	_prevRenderingEnabled = false;
	_renderingEnabled = false;

	_ignoreVramRead = 0;
	_openBus = 0;
	memset(_openBusDecayStamp, 0, sizeof(_openBusDecayStamp));

	_tmpVideoRamAddr = 0;
	_highBitShift = 0;
	_lowBitShift = 0;
	_spriteRamAddr = 0;
	_xScroll = 0;
	_writeToggle = false;

	_control = {};
	_mask = {};

	if(!softReset) {
		//"The VBL flag (PPUSTATUS bit 7) is random at power, and unchanged by reset."
		_statusFlags = {};
		_statusFlags.VerticalBlank = _settings->GetNesConfig().RandomizeMapperPowerOnState ? _settings->GetRandomBool() : false;
	}

	_tile = {};
	_currentTilePalette = 0;
	_previousTilePalette = 0;

	_ppuBusAddress = 0;
	_intensifyColorBits = 0;
	_paletteRamMask = 0x3F;
	_lastUpdatedPixel = -1;
	_lastSprite = nullptr;
	_oamCopybuffer = 0;
	_spriteInRange = false;
	_sprite0Added = false;
	_oamCopyDone = false;

	memset(_spriteTiles, 0, sizeof(_spriteTiles));
	_spriteCount = 0;
	_secondaryOamAddr = 0;
	_sprite0Visible = false;
	_spriteIndex = 0;

	//First execution will be cycle 0, scanline 0
	_scanline = -1;
	_cycle = 340;

	_frameCount = 1;
	_memoryReadBuffer = 0;

	_overflowBugCounter = 0;

	_updateVramAddrDelay = 0;
	_updateVramAddr = 0;

	_firstVisibleSpriteAddr = 0;
	_lastVisibleSpriteAddr = 0;

	_allowFullPpuAccess = false;

	UpdateMinimumDrawCycles();
}

template<class T> void NesPpu<T>::UpdateTimings(ConsoleRegion region, bool overclockAllowed)
{
	_region = region;

	switch(_region) {
		case ConsoleRegion::Auto:
			//Should never be Auto
			break;

		case ConsoleRegion::Ntsc:
			_nmiScanline = 241;
			_vblankEnd = 260;
			_standardNmiScanline = 241;
			_standardVblankEnd = 260;
			_masterClockDivider = 4;
			break;
		case ConsoleRegion::Pal:
			_nmiScanline = 241;
			_vblankEnd = 310;
			_standardNmiScanline = 241;
			_standardVblankEnd = 310;
			_masterClockDivider = 5;
			break;
		case ConsoleRegion::Dendy:
			_nmiScanline = 291;
			_vblankEnd = 310;
			_standardNmiScanline = 291;
			_standardVblankEnd = 310;
			_masterClockDivider = 5;
			break;
	}

	if(overclockAllowed) {
		NesConfig& cfg = _console->GetNesConfig();
		_nmiScanline += cfg.PpuExtraScanlinesBeforeNmi;
		_standardVblankEnd += cfg.PpuExtraScanlinesBeforeNmi;
		_vblankEnd += cfg.PpuExtraScanlinesAfterNmi + cfg.PpuExtraScanlinesBeforeNmi;
	}

	_palSpriteEvalScanline = _nmiScanline + 24;
}

template<class T> void NesPpu<T>::UpdateVideoRamAddr()
{
	if(_scanline >= 240 || !IsRenderingEnabled()) {
		_videoRamAddr = (_videoRamAddr + (_control.VerticalWrite ? 32 : 1)) & 0x7FFF;

		//Trigger memory read when setting the vram address - needed by MMC3 IRQ counter
		//"Should be clocked when A12 changes to 1 via $2007 read/write"
		SetBusAddress(_videoRamAddr & 0x3FFF);

		if(!_renderingEnabled) {
			_emu->AddDebugEvent<CpuType::Nes>(DebugEventType::BgColorChange);
		}
	} else {
		//"During rendering (on the pre-render line and the visible lines 0-239, provided either background or sprite rendering is enabled), "
		//it will update v in an odd way, triggering a coarse X increment and a Y increment simultaneously"
		IncHorizontalScrolling();
		IncVerticalScrolling();
	}
}

template<class T> void NesPpu<T>::SetOpenBus(uint8_t mask, uint8_t value)
{
	//Decay expired bits, set new bits and update stamps on each individual bit
	if(mask == 0xFF) {
		//Shortcut when mask is 0xFF - all bits are set to the value and stamps updated
		_openBus = value;
		for(int i = 0; i < 8; i++) {
			_openBusDecayStamp[i] = _frameCount;
		}
	} else {
		uint16_t openBus = (_openBus << 8);
		for(int i = 0; i < 8; i++) {
			openBus >>= 1;
			if(mask & 0x01) {
				if(value & 0x01) {
					openBus |= 0x80;
				} else {
					openBus &= 0xFF7F;
				}
				_openBusDecayStamp[i] = _frameCount;
			} else if(_frameCount - _openBusDecayStamp[i] > 3) {
				//Decay bits to 0 after 3 frames
				//This is a very conservative estimate, individual bits tend to decay
				//much faster than this.
				openBus &= 0xFF7F;
			}
			value >>= 1;
			mask >>= 1;
		}

		_openBus = (uint8_t)openBus;
	}
}

template<class T> uint8_t NesPpu<T>::ApplyOpenBus(uint8_t mask, uint8_t value)
{
	SetOpenBus(~mask, value);
	return value | (_openBus & mask);
}

template<class T> PpuModel NesPpu<T>::GetPpuModel()
{
	if(_mapper->GetGameSystem() == GameSystem::VsSystem) {
		return _mapper->GetPpuModel();
	} else {
		return PpuModel::Ppu2C02;
	}
}

template<class T> void NesPpu<T>::ProcessStatusRegOpenBus(uint8_t& openBusMask, uint8_t& returnValue)
{
	switch(GetPpuModel()) {
		case PpuModel::Ppu2C05A:
			openBusMask = 0x00;
			returnValue |= 0x1B;
			break;

		case PpuModel::Ppu2C05B:
			openBusMask = 0x00;
			returnValue |= 0x3D;
			break;

		case PpuModel::Ppu2C05C:
			openBusMask = 0x00;
			returnValue |= 0x1C;
			break;

		case PpuModel::Ppu2C05D:
			openBusMask = 0x00;
			returnValue |= 0x1B;
			break;

		case PpuModel::Ppu2C05E:
			openBusMask = 0x00;
			break;

		default: break;
	}
}

template<class T> uint8_t NesPpu<T>::PeekRam(uint16_t addr)
{
	//Used by debugger to get register values without side-effects (heavily edited copy of ReadRAM)
	uint8_t openBusMask = 0xFF;
	uint8_t returnValue = 0;
	switch(GetRegisterID(addr)) {
		case PpuRegisters::Status:
			returnValue = ((uint8_t)_statusFlags.SpriteOverflow << 5) | ((uint8_t)_statusFlags.Sprite0Hit << 6) | ((uint8_t)_statusFlags.VerticalBlank << 7);
			if(_scanline == _nmiScanline && _cycle < 3) {
				//Clear vertical blank flag
				returnValue &= 0x7F;
			}
			openBusMask = 0x1F;
			ProcessStatusRegOpenBus(openBusMask, returnValue);
			break;

		case PpuRegisters::SpriteData:
			if(!_console->GetNesConfig().DisablePpu2004Reads) {
				if(_scanline <= 239 && IsRenderingEnabled()) {
					if((_cycle >= 257 && _cycle <= 340) || _cycle == 0) {
						returnValue = _secondarySpriteRam[_secondaryOamAddr & 0x1F];
					}
					returnValue = _oamCopybuffer;
				} else {
					returnValue = _spriteRam[_spriteRamAddr];
				}
				openBusMask = 0x00;
			}
			break;

		case PpuRegisters::VideoMemoryData:
			returnValue = _memoryReadBuffer;

			if((_videoRamAddr & 0x3FFF) >= 0x3F00 && !_console->GetNesConfig().DisablePaletteRead) {
				returnValue = (ReadPaletteRam(_videoRamAddr) & _paletteRamMask) | (_openBus & 0xC0);
				openBusMask = 0xC0;
			} else {
				openBusMask = 0x00;
			}
			break;

		default:
			break;
	}
	return returnValue | (_openBus & openBusMask);
}

template<class T> uint8_t NesPpu<T>::ReadRam(uint16_t addr)
{
	uint8_t openBusMask = 0xFF;
	uint8_t returnValue = 0;
	switch(GetRegisterID(addr)) {
		case PpuRegisters::Status:
			_writeToggle = false;
			returnValue =
				((uint8_t)_statusFlags.SpriteOverflow << 5) |
				((uint8_t)_statusFlags.Sprite0Hit << 6) |
				((uint8_t)_statusFlags.VerticalBlank << 7);

			UpdateStatusFlag();
			openBusMask = 0x1F;

			ProcessStatusRegOpenBus(openBusMask, returnValue);
			break;

		case PpuRegisters::SpriteData:
			if(!_console->GetNesConfig().DisablePpu2004Reads) {
				if(_scanline <= 239 && IsRenderingEnabled()) {
					//While the screen is being drawn
					if((_cycle >= 257 && _cycle <= 340) || _cycle == 0) {
						//If we're doing sprite rendering, set OAM copy buffer to its proper value.  This is done here for performance.
						//It's faster to only do this here when it's needed, rather than splitting LoadSpriteTileInfo() into an 8-step process
						_oamCopybuffer = _secondarySpriteRam[_secondaryOamAddr & 0x1F];
					}
					//Return the value that PPU is currently using for sprite evaluation/rendering
					returnValue = _oamCopybuffer;
				} else {
					returnValue = ReadSpriteRam(_spriteRamAddr);
					_emu->ProcessPpuRead<CpuType::Nes>(_spriteRamAddr, returnValue, MemoryType::NesSpriteRam);
				}
				openBusMask = 0x00;
			}
			break;

		case PpuRegisters::VideoMemoryData:
			if(!_allowFullPpuAccess && _settings->GetNesConfig().RestrictPpuAccessOnFirstFrame) {
				openBusMask = 0x00;
				returnValue = 0;
			} else if(_ignoreVramRead) {
				//2 reads to $2007 in quick succession (2 consecutive CPU cycles) causes the 2nd read to be ignored (normally depends on PPU/CPU timing, but this is the simplest solution)
				//Return open bus in this case? (which will match the last value read)
				openBusMask = 0xFF;
			} else {
				returnValue = _memoryReadBuffer;
				// The PPU Read Buffer is not updated until the CPU read ends, and 2 more ppu cycles have passed.
				_ppuMemoryDataReadStateMachine = 5;

				if((_ppuBusAddress & 0x3FFF) >= 0x3F00 && !_console->GetNesConfig().DisablePaletteRead) {
					//Note: When grayscale is turned on, the read values also have the grayscale mask applied to them
					returnValue = (ReadPaletteRam(_ppuBusAddress) & _paletteRamMask) | (_openBus & 0xC0);
					_emu->ProcessPpuRead<CpuType::Nes>(_ppuBusAddress, returnValue, MemoryType::NesPpuMemory);
					openBusMask = 0xC0;
				} else {
					openBusMask = 0x00;
				}

				if(_scanline < 240 && IsRenderingEnabled()) {
					_emu->BreakIfDebugging(CpuType::Nes, BreakSource::NesInvalidVramAccess);
				}

				_ignoreVramRead = 6;
				_needStateUpdate = true;
			}
			break;

		default:
			break;
	}
	return ApplyOpenBus(openBusMask, returnValue);
}

template<class T> void NesPpu<T>::WriteRam(uint16_t addr, uint8_t value)
{
	if(addr != 0x4014) {
		SetOpenBus(0xFF, value);
	}

	switch(GetRegisterID(addr)) {
		case PpuRegisters::Control:
			if(GetPpuModel() >= PpuModel::Ppu2C05A && GetPpuModel() <= PpuModel::Ppu2C05E) {
				SetMaskRegister(value);
			} else {
				SetControlRegister(value);
			}
			break;

		case PpuRegisters::Mask:
			if(GetPpuModel() >= PpuModel::Ppu2C05A && GetPpuModel() <= PpuModel::Ppu2C05E) {
				SetControlRegister(value);
			} else {
				SetMaskRegister(value);
			}
			break;

		case PpuRegisters::SpriteAddr:
			//If OAM1ADDR is selected, updating the address can corrupt 2 rows (the open bus row and the destination row).
			//This is actually alignment-dependent, but we're currently modeling worst-case behavior.
			if(_settings->GetNesConfig().EnablePpuOamRowCorruption && _region == ConsoleRegion::Ntsc && (!_prevRenderingEnabled || _scanline >= 240 || (_cycle < 257 && (_cycle & 1)))) {
				uint8_t sourceRow = _spriteRamAddr >> 3;
				uint8_t openBusRow = _console->GetMemoryManager()->GetOpenBus() >> 3;
				uint8_t destRow = value >> 3;
				CorruptOamRow(sourceRow, openBusRow);
				CorruptOamRow(openBusRow, destRow);
			}
			//TODO There is some kind of delay on committing the $2003 value on PAL (this is how it fixes the corruption issue).
			//This currently isn't emulated.
			//"In the 2C02, writes to $2003 took effect immediately. In the 2C07, they're buffered through pclk0+pclk1+pclk0[+pclk1+pclk0].
			//I'm guessing that's what it took to prevent corruption. (it's actually pclk0+pclk1+pclk0 NOR pclk0+pclk1+pclk0+pclk1+pclk0)"
			_spriteRamAddr = value;
			break;

		case PpuRegisters::SpriteData:
			//TODO: The $2003 increment can be eaten if it happens at the same time as another increment (from sprite processing or PAL's forced refresh).
			//The increment appears to happen 1.5-2.5 dots after the write ends.
			if(_scanline >= 240 || !IsRenderingEnabled()) {
				if((_spriteRamAddr & 0x03) == 0x02) {
					//"The three unimplemented bits of each sprite's byte 2 do not exist in the PPU and always read back as 0 on PPU revisions that allow reading PPU OAM through OAMDATA ($2004)"
					value &= 0xE3;
				}
				WriteSpriteRam(_spriteRamAddr, value);
				_emu->ProcessPpuWrite<CpuType::Nes>(_spriteRamAddr, value, MemoryType::NesSpriteRam);
				if(_region == ConsoleRegion::Ntsc || _scanline < _palSpriteEvalScanline || ((_cycle & 1) && (_cycle != 339))) {
					_spriteRamAddr++;
				}
			} else {
				//"Writes to OAMDATA during rendering (on the pre-render line and the visible lines 0-239, provided either sprite or background rendering is enabled) do not modify values in OAM,
				//but do perform a glitchy increment of OAMADDR, bumping only the high 6 bits"
				//TODO: This should actually write the contents of the OAM buffer to the current OAM1 or OAM2 byte, 1.0-2.0 dots after the write ends.
				_spriteRamAddr = (_spriteRamAddr + 4) & 0xFC;
				_emu->BreakIfDebugging(CpuType::Nes, BreakSource::NesInvalidOamWrite);
			}
			break;

		case PpuRegisters::ScrollOffsets:
			if(!_allowFullPpuAccess && _settings->GetNesConfig().RestrictPpuAccessOnFirstFrame) {
				return;
			}

			if(_writeToggle) {
				_tmpVideoRamAddr = (_tmpVideoRamAddr & ~0x73E0) | ((value & 0xF8) << 2) | ((value & 0x07) << 12);
			} else {
				_xScroll = value & 0x07;

				uint16_t newAddr = (_tmpVideoRamAddr & ~0x001F) | (value >> 3);
				ProcessTmpAddrScrollGlitch(newAddr, _console->GetMemoryManager()->GetOpenBus() >> 3, 0x001F);
			}
			_writeToggle = !_writeToggle;
			break;

		case PpuRegisters::VideoMemoryAddr:
			if(!_allowFullPpuAccess && _settings->GetNesConfig().RestrictPpuAccessOnFirstFrame) {
				return;
			}

			if(_writeToggle) {
				_tmpVideoRamAddr = (_tmpVideoRamAddr & ~0x00FF) | value;

				//Video RAM update is apparently delayed by 3 PPU cycles (based on Visual NES findings)
				_needStateUpdate = true;
				_updateVramAddrDelay = 3;
				_updateVramAddr = _tmpVideoRamAddr;
			} else {
				uint16_t newAddr = (_tmpVideoRamAddr & ~0xFF00) | ((value & 0x3F) << 8);
				ProcessTmpAddrScrollGlitch(newAddr, _console->GetMemoryManager()->GetOpenBus() << 8, 0x0C00);
			}
			_writeToggle = !_writeToggle;
			break;

		case PpuRegisters::VideoMemoryData:
			// The write to VRAM does not occur until the CPU write ends, and 2 more ppu cycles have passed.
			_ppuMemoryDataWriteStateMachine = 5;
			_ppuMemoryDataWriteLatch = value;
			_needStateUpdate = true;
			break;

		case PpuRegisters::SpriteDMA:
			_console->GetCpu()->RunDMATransfer(value);
			break;

		default:
			break;
	}
}

template<class T> void NesPpu<T>::ProcessTmpAddrScrollGlitch(uint16_t normalAddr, uint16_t value, uint16_t mask)
{
	_tmpVideoRamAddr = normalAddr;
	if(_cycle == 257 && _console->GetNesConfig().EnablePpu2000ScrollGlitch && _scanline < 240 && IsRenderingEnabled()) {
		//Use open bus to set some parts of V (glitch that occurs when writing to $2000/$2005/$2006 on cycle 257)
		_videoRamAddr = (_videoRamAddr & ~mask) | (value & mask);
		_emu->BreakIfDebugging(CpuType::Nes, BreakSource::NesBreakOnPpuScrollGlitch);
	}
}

template<class T> void NesPpu<T>::SetControlRegister(uint8_t value)
{
	if(!_allowFullPpuAccess && _settings->GetNesConfig().RestrictPpuAccessOnFirstFrame) {
		return;
	}

	uint8_t nameTable = (value & 0x03);

	uint16_t normalAddr = (_tmpVideoRamAddr & ~0x0C00) | (nameTable << 10);
	ProcessTmpAddrScrollGlitch(normalAddr, _console->GetMemoryManager()->GetOpenBus() << 10, 0x0400);

	_control.VerticalWrite = (value & 0x04) == 0x04;
	_control.SpritePatternAddr = ((value & 0x08) == 0x08) ? 0x1000 : 0x0000;
	_control.BackgroundPatternAddr = ((value & 0x10) == 0x10) ? 0x1000 : 0x0000;
	_control.LargeSprites = (value & 0x20) == 0x20;

	_control.SecondaryPpu = (value & 0x40) == 0x40;
	if(_control.SecondaryPpu) {
		_emu->BreakIfDebugging(CpuType::Nes, BreakSource::NesBreakOnExtOutputMode);
	}
	_control.NmiOnVerticalBlank = (value & 0x80) == 0x80;

	//"By toggling NMI_output ($2000 bit 7) during vertical blank without reading $2002, a program can cause /NMI to be pulled low multiple times, causing multiple NMIs to be generated."
	if(!_control.NmiOnVerticalBlank) {
		_console->GetCpu()->ClearNmiFlag();
	} else if(_control.NmiOnVerticalBlank && _statusFlags.VerticalBlank) {
		_console->GetCpu()->SetNmiFlag();
	}
}

template<class T> void NesPpu<T>::SetMaskRegister(uint8_t value)
{
	if(!_allowFullPpuAccess && _settings->GetNesConfig().RestrictPpuAccessOnFirstFrame) {
		return;
	}

	_mask.Grayscale = (value & 0x01) == 0x01;
	_mask.BackgroundMask = (value & 0x02) == 0x02;
	_mask.SpriteMask = (value & 0x04) == 0x04;
	_mask.BackgroundEnabled = (value & 0x08) == 0x08;
	_mask.SpritesEnabled = (value & 0x10) == 0x10;
	_mask.IntensifyBlue = (value & 0x80) == 0x80;

	if(_region == ConsoleRegion::Ntsc) {
		_mask.IntensifyRed = (value & 0x20) == 0x20;
		_mask.IntensifyGreen = (value & 0x40) == 0x40;
	} else if(_region == ConsoleRegion::Pal || _region == ConsoleRegion::Dendy) {
		//"Note that on the Dendy and PAL NES, the green and red bits swap meaning."
		_mask.IntensifyRed = (value & 0x40) == 0x40;
		_mask.IntensifyGreen = (value & 0x20) == 0x20;
	}

	if(_renderingEnabled != (_mask.BackgroundEnabled | _mask.SpritesEnabled)) {
		_needStateUpdate = true;
	}

	UpdateMinimumDrawCycles();
	UpdateGrayscaleAndIntensifyBits();

	_emu->AddDebugEvent<CpuType::Nes>(DebugEventType::BgColorChange);
}

template<class T> void NesPpu<T>::UpdateStatusFlag()
{
	_statusFlags.VerticalBlank = false;
	_console->GetCpu()->ClearNmiFlag();

	if(_scanline == _nmiScanline && _cycle == 0) {
		//"Reading one PPU clock before reads it as clear and never sets the flag or generates NMI for that frame."
		_preventVblFlag = true;
	}
}

//Taken from http://wiki.nesdev.com/w/index.php/The_skinny_on_NES_scrolling#Wrapping_around
template<class T> void NesPpu<T>::IncVerticalScrolling()
{
	uint16_t addr = _videoRamAddr;

	if((addr & 0x7000) != 0x7000) {
		// if fine Y < 7
		addr += 0x1000; // increment fine Y
	} else {
		// fine Y = 0
		addr &= ~0x7000;
		int y = (addr & 0x03E0) >> 5; // let y = coarse Y
		if(y == 29) {
			y = 0; // coarse Y = 0
			addr ^= 0x0800; // switch vertical nametable
		} else if(y == 31) {
			y = 0; // coarse Y = 0, nametable not switched
		} else {
			y++; // increment coarse Y
		}
		addr = (addr & ~0x03E0) | (y << 5); // put coarse Y back into v
	}
	_videoRamAddr = addr;
}

//Taken from http://wiki.nesdev.com/w/index.php/The_skinny_on_NES_scrolling#Wrapping_around
template<class T> void NesPpu<T>::IncHorizontalScrolling()
{
	//Increase coarse X scrolling value.
	uint16_t addr = _videoRamAddr;
	if((addr & 0x001F) == 31) {
		//When the value is 31, wrap around to 0 and switch nametable
		addr = (addr & ~0x001F) ^ 0x0400;
	} else {
		addr++;
	}
	_videoRamAddr = addr;
}

//Taken from http://wiki.nesdev.com/w/index.php/The_skinny_on_NES_scrolling#Tile_and_attribute_fetching
template<class T> uint16_t NesPpu<T>::GetNameTableAddr()
{
	return 0x2000 | (_videoRamAddr & 0x0FFF);
}

//Taken from http://wiki.nesdev.com/w/index.php/The_skinny_on_NES_scrolling#Tile_and_attribute_fetching
template<class T> uint16_t NesPpu<T>::GetAttributeAddr()
{
	return 0x23C0 | (_videoRamAddr & 0x0C00) | ((_videoRamAddr >> 4) & 0x38) | ((_videoRamAddr >> 2) & 0x07);
}

template<class T> void NesPpu<T>::SetBusAddress(uint16_t addr)
{
	_ppuBusAddress = addr;
	if(_mapper->HasVramAddressHook()) {
		_mapper->NotifyVramAddressChange(addr);
	}
}

template<class T> uint8_t NesPpu<T>::ReadVram(uint16_t addr, MemoryOperationType type)
{
	SetBusAddress(addr);
	return _mapper->ReadVram(addr, type);
}

template<class T> void NesPpu<T>::WriteVram(uint16_t addr, uint8_t value)
{
	SetBusAddress(addr);
	_mapper->WriteVram(addr, value);
}

template<class T> void NesPpu<T>::LoadTileInfo()
{
	switch(_cycle & 0x07) {
		case 0:
			//This is the last dot of the 8-dot sequence (e.g dot 8, 16, etc.)
			//Replace bottom 8 bits of the BG shifters
			_highBitShift &= 0xFF00;
			_highBitShift |= _tile.HighByte;
			_lowBitShift &= 0xFF00;
			_lowBitShift |= _tile.LowByte;

			_previousTilePalette = _currentTilePalette;
			_currentTilePalette = _tile.PaletteOffset;
			((T*)this)->PushTileInformation(); //Used by HD packs
			break;

		case 1: {
			uint8_t tileIndex = ReadVram(GetNameTableAddr());
			_tile.TileAddr = (tileIndex << 4) | (_videoRamAddr >> 12) | _control.BackgroundPatternAddr;
			((T*)this)->StoreTileInformation(); //Used by HD packs
			break;
		}

		case 3: {
			uint8_t shift = ((_videoRamAddr >> 4) & 0x04) | (_videoRamAddr & 0x02);
			_tile.PaletteOffset = ((ReadVram(GetAttributeAddr()) >> shift) & 0x03) << 2;
			break;
		}

		case 5:
			_tile.LowByte = ReadVram(_tile.TileAddr);
			break;

		case 7:
			_tile.HighByte = ReadVram(_tile.TileAddr + 8);
			break;
	}
}

template<class T> void NesPpu<T>::LoadSprite(uint8_t spriteY, uint8_t tileIndex, uint8_t attributes, uint8_t spriteX, bool extraSprite)
{
	bool backgroundPriority = (attributes & 0x20) == 0x20;
	bool horizontalMirror = (attributes & 0x40) == 0x40;
	bool verticalMirror = (attributes & 0x80) == 0x80;

	uint16_t tileAddr;
	const uint8_t spriteSizeMask = _control.LargeSprites ? 15 : 7;

	//This function is only called on rendering scanlines (0-239) or pre-render (-1). We need to handle the pre-render scanline numbers manually.
	//These are deliberately truncated to 8 bits to match hardware behavior.
	const uint8_t scanline8Bit = (uint8_t)(_scanline >= 0 ? _scanline : (_region == ConsoleRegion::Ntsc ? 261 : 311));
	uint16_t rangeResult = scanline8Bit - spriteY;
	if(verticalMirror) {
		rangeResult ^= spriteSizeMask;
	}

	if(_control.LargeSprites) {
		tileAddr = (((tileIndex & 0x01) << 12) | ((tileIndex & ~0x01) << 4)) + ((rangeResult & 0x08) << 1) + (rangeResult & 0x07);
	} else {
		tileAddr = (_control.SpritePatternAddr | (tileIndex << 4)) + (rangeResult & 0x07);
	}

	NesSpriteInfo& info = _spriteTiles[_spriteIndex];
	info.BackgroundPriority = backgroundPriority;
	info.PaletteOffset = ((attributes & 0x03) << 2) | 0x10;
	if(extraSprite) {
		//Use DebugReadVram for extra sprites to prevent side-effects.
		info.LowByte = _mapper->DebugReadVram(tileAddr);
		info.HighByte = _mapper->DebugReadVram(tileAddr + 8);
	} else {
		info.LowByte = ReadVram(tileAddr);
		info.HighByte = ReadVram(tileAddr + 8);
	}
	info.SpriteX = spriteX;

	bool inRange = rangeResult <= spriteSizeMask;
	if(inRange) {
		if(horizontalMirror) {
			info.LowByte = BitUtilities::ReverseByte(info.LowByte);
			info.HighByte = BitUtilities::ReverseByte(info.HighByte);
		}

		((T*)this)->StoreSpriteInformation(horizontalMirror, verticalMirror, tileAddr, rangeResult, info); //Used by HD packs

		if(!extraSprite) {
			_spriteShifterList[_spriteIndex] = (((uint16_t)spriteX + 1) << 4) | _spriteIndex;

			//Clear expired flag to indicate that the counter was reloaded
			_expiredSpriteShifters &= ~(1 << _spriteIndex);
		}

		_spriteCount++;
		_processSprites = true;
	} else if(!extraSprite) {
		info.LowByte = 0;
		info.HighByte = 0;

		_spriteShifterList[_spriteIndex] = BaseNesPpu::SpriteShifterDone;
	}

	_spriteIndex++;
}

template<class T> void NesPpu<T>::LoadExtraSprites()
{
	if(_spriteCount == 8 && ((T*)this)->RemoveSpriteLimit() && _scanline >= 0) {
		bool loadExtraSprites = true;

		if(((T*)this)->UseAdaptiveSpriteLimit()) {
			uint16_t lastPosition = 0xFFFF;
			uint8_t identicalSpriteCount = 0;
			uint8_t maxIdenticalSpriteCount = 0;
			for(int i = 0; i < 64; i++) {
				uint8_t y = _spriteRam[i << 2];
				if(_scanline >= y && _scanline < y + (_control.LargeSprites ? 16 : 8)) {
					uint8_t x = _spriteRam[(i << 2) + 3];
					uint16_t position = (y << 8) | x;
					if(lastPosition != position) {
						if(identicalSpriteCount > maxIdenticalSpriteCount) {
							maxIdenticalSpriteCount = identicalSpriteCount;
						}
						lastPosition = position;
						identicalSpriteCount = 1;
					} else {
						identicalSpriteCount++;
					}
				}
			}
			loadExtraSprites = identicalSpriteCount < 8 && maxIdenticalSpriteCount < 8;
		}

		if(loadExtraSprites) {
			for(uint32_t i = (_lastVisibleSpriteAddr + 4) & 0xFC; i != (uint32_t)(_firstVisibleSpriteAddr & 0xFC); i = (i + 4) & 0xFC) {
				uint8_t spriteY = _spriteRam[i];
				if(_scanline >= spriteY && _scanline < spriteY + (_control.LargeSprites ? 16 : 8)) {
					LoadSprite(spriteY, _spriteRam[i + 1], _spriteRam[i + 2], _spriteRam[i + 3], true);
				}
			}
		}
	}
}

template<class T> void NesPpu<T>::LoadSpriteTileInfo()
{
	//Set it to whatever shifter we're filling.
	_spriteIndex = (_cycle - 257) >> 3;
	uint8_t* spriteAddr = _secondarySpriteRam + _spriteIndex * 4;
	LoadSprite(*spriteAddr, *(spriteAddr + 1), *(spriteAddr + 2), *(spriteAddr + 3), false);
}

template<class T> void NesPpu<T>::ShiftTileRegisters()
{
	_lowBitShift <<= 1;
	_highBitShift <<= 1;

	//The high bit shifter is filled with 1s as it shifts (the low bit shifter is not)
	_highBitShift |= 1;
}

template<class T> uint8_t NesPpu<T>::GetPixelColor()
{
	uint8_t offset = _xScroll;
	uint8_t backgroundColor = 0;
	uint8_t spriteBgColor = 0;

	if(_cycle > _minimumDrawBgCycle) {
		//BackgroundMask = false: Hide background in leftmost 8 pixels of screen
		spriteBgColor = (((_lowBitShift << offset) & 0x8000) >> 15) | (((_highBitShift << offset) & 0x8000) >> 14);
		if(_emulatorBgEnabled) {
			backgroundColor = spriteBgColor;
		}
	}

	int8_t spriteIndex = -1;
	uint8_t spriteColor = 0;
	if(_processSprites && _prevRenderingEnabled) {
		//If the dot is skipped, all sprite shifters are active on the first dot of the scanline.
		uint8_t remainingShifters = _dotSkipped ? 0xFF : _activeSpriteShifters;

		_lastSprite = &_spriteTiles[BitUtilities::GetHighestBitIndex(_activeSpriteShifters)];

		//Output+shift all active sprite shifters.
		while(remainingShifters) {
			uint8_t i = BitUtilities::GetHighestBitIndex(remainingShifters);
			remainingShifters &= ~(1 << i);

			NesSpriteInfo& sprite = _spriteTiles[i];

			//Get the color. We start with the lowest priority shifter first so the higher priority ones override it.
			uint8_t currColor = ((sprite.HighByte >> 6) & 0x2) | (sprite.LowByte >> 7);
			if(currColor != 0) {
				spriteIndex = i;
				spriteColor = currColor;
				_lastSprite = &sprite;
			}
			sprite.HighByte <<= 1;
			sprite.LowByte <<= 1;

			//If the shifter is empty, deactivate it.
			if(!(sprite.HighByte | sprite.LowByte)) {
				_activeSpriteShifters &= ~(1 << i);
				UpdateProcessSpritesFlag();
			}
		}

		if(_cycle > _minimumDrawSpriteCycle && _mask.SpritesEnabled) {
			if(_spriteCount > 8 && spriteColor == 0) {
				for(spriteIndex = 8; spriteIndex < _spriteCount; spriteIndex++) {
					NesSpriteInfo& sprite = _spriteTiles[spriteIndex];
					uint32_t shift = (int32_t)_cycle - sprite.SpriteX - 1;
					if(shift < 8) {
						_lastSprite = &sprite;
						spriteColor = ((sprite.LowByte << shift) & 0x80) >> 7 | ((sprite.HighByte << shift) & 0x80) >> 6;
						if(spriteColor) {
							break;
						}
					}
				}
			}

			if(spriteColor != 0) {
				if(_sprite0Visible && spriteIndex == 0 && spriteColor != 0 && spriteBgColor != 0 && _cycle != 256 && _mask.BackgroundEnabled && !_statusFlags.Sprite0Hit && _cycle > _minimumDrawSpriteStandardCycle) {
					//"The hit condition is basically sprite zero is in range AND the first sprite output unit is outputting a non-zero pixel AND the background drawing unit is outputting a non-zero pixel."
					//"Sprite zero hits do not register at x=255" (cycle 256)
					//"... provided that background and sprite rendering are both enabled"
					//"Should always miss when Y >= 239"
					_statusFlags.Sprite0Hit = true;

					_emu->AddDebugEvent<CpuType::Nes>(DebugEventType::SpriteZeroHit);
				}

				if(_emulatorSpritesEnabled && (backgroundColor == 0 || !_spriteTiles[spriteIndex].BackgroundPriority)) {
					//Check sprite priority
					return _spriteTiles[spriteIndex].PaletteOffset + spriteColor;
				}
			}
		}
	}
	return ((offset + ((_cycle - 1) & 0x07) < 8) ? _previousTilePalette : _currentTilePalette) + backgroundColor;
}

template<class T> void NesPpu<T>::ProcessRenderingDisabledPixel()
{
	//Duplicated logic from ProcessScanlineImpl, only called when rendering is disabled
	if(_scanline >= 0) {
		ProcessSpriteShifters();
		((T*)this)->DrawPixel();
	} else if(_cycle == 1) {
		_statusFlags.VerticalBlank = false;
		_console->GetCpu()->ClearNmiFlag();
	}
}

template<class T> void NesPpu<T>::ProcessScanlineImpl()
{
	//Only called for cycle 1+
	if(_cycle <= 256) {
		if(_prevRenderingEnabled) {
			//Process visible pixels when rendering is enabled
			//Portions are duplicated in ProcessRenderingDisabledPixel for performance reasons
			if(_scanline >= 0) {
				//"Secondary OAM clear and sprite evaluation do not occur on the pre-render line"
				ProcessSpriteShifters();

				((T*)this)->DrawPixel();

				ProcessSpriteEvaluation();
				ShiftTileRegisters();
			} else if(_cycle == 1) {
				//Pre-render scanline logic
				_statusFlags.VerticalBlank = false;
				_console->GetCpu()->ClearNmiFlag();
			}

			if((_cycle & 0x07) == 0) {
				IncHorizontalScrolling();
				if(_cycle == 256) {
					IncVerticalScrolling();
				}
			}
			LoadTileInfo();
		} else {
			ProcessRenderingDisabledPixel();
		}
	} else if(_cycle >= 257 && _cycle <= 320) {
		if(_prevRenderingEnabled) {
			//sprite_0_on_next_scanline is copied to sprite_0_on_this_scanline every dot in this range.
			_sprite0Visible = _sprite0Added;

			//"OAMADDR is set to 0 during each of ticks 257-320 (the sprite tile loading interval) of the pre-render and visible scanlines." (When rendering)
			_spriteRamAddr = 0;

			switch((_cycle - 257) % 8) {
				//Garbage NT sprite fetch (257, 265, 273, etc.) - Required for proper MC-ACC IRQs (MMC3 clone)
				case 0: ReadVram(GetNameTableAddr()); break;

				//Garbage NT sprite fetch
				case 2: ReadVram(GetNameTableAddr()); break;

				//Cycle 260, 268, etc.  This is an approximation (each tile is actually loaded in 8 steps (e.g from 257 to 264))
				case 4: LoadSpriteTileInfo(); break;
			}

			if(_scanline == -1 && _cycle >= 280 && _cycle <= 304) {
				//copy vertical scrolling value from t
				_videoRamAddr = (_videoRamAddr & ~0x7BE0) | (_tmpVideoRamAddr & 0x7BE0);
			}
			// Load the extra sprites before tile loading starts (which matters for MMC5)
			// The CHR banks are switched back to the bg tileset in LoadTileInfo on cycle 321
			// and the extra sprites will potentially read from the wrong banks
			if(_cycle == 320) {
				LoadExtraSprites();
			}
			//Increment OAM2ADDR during the first 4 dots of each set of 8. There is special behavior where the first set of 8 skips the
			//first increment (because OAM2ADDR is being cleared), and an extra increment is done after the last set of 8.
			_secondaryOamAddr += !((_cycle - 1) & 4);
		}
		if(_cycle == 257) {
			_spriteIndex = 0;
			_spriteCount = 0;
			if(_prevRenderingEnabled) {
				//copy horizontal scrolling value from t
				_videoRamAddr = (_videoRamAddr & ~0x041F) | (_tmpVideoRamAddr & 0x041F);

				//This is reset during dots 256.5-257.0, overriding the increment that would happen in 257.
				_secondaryOamAddr = 0;
			}
		}
	} else if(_cycle >= 321 && _cycle <= 336) {
		if(_prevRenderingEnabled) {
			if(_cycle == 321) {
				//Do the final increment, which follows the last set of 8 sprite fetch dots.
				_secondaryOamAddr++;
			} else if(_prevRenderingEnabled && (_cycle == 328 || _cycle == 336)) {
				IncHorizontalScrolling();
			}
			ShiftTileRegisters();
			LoadTileInfo();
		}
	} else if(_cycle == 337) {
		if(IsRenderingEnabled()) {
			_tile.TileAddr = ReadVram(GetNameTableAddr());
		}
	} else if(_cycle == 339) {
		_activeSpriteShifters = 0;

		if(IsRenderingEnabled()) {
			_tile.TileAddr = ReadVram(GetNameTableAddr());

			//Set sprite shifters to "count" mode (only if rendering is enabled)
			//Only do this if the shifter did not finish counting already (which can happen in
			//some scenarios when rendering is disabled mid-scanline, etc.)
			for(int i = 0; i < 8; i++) {
				uint8_t bit = (1 << (_spriteShifterList[i] & 0x07));
				if(_spriteShifterList[i] != NesPpu::SpriteShifterDone && !(_expiredSpriteShifters & bit)) {
					_countingSpriteShifters |= bit;
				}
			}

			if(_scanline == -1 && _cycle == 339 && (_frameCount & 0x01) && _region == ConsoleRegion::Ntsc && GetPpuModel() == PpuModel::Ppu2C02) {
				//This behavior is NTSC-specific - PAL frames are always the same number of cycles
				//"With rendering enabled, each odd PPU frame is one PPU clock shorter than normal" (skip from 339 to 0, going over 340)
				_cycle = 340;

				//Set a counter for this so it'll be nonzero for GetPixelColor on dot 1, forcing sprite shifter output.
				_dotSkipped = 3;
				_needStateUpdate = true;
				// Delay all sprites by 1 pixel.
				for(int i = 0; i < 8; i++) {
					_spriteShifterList[i] += 1 << 4;
				}
				_nextSpriteShifterCycle++;
			}
		}

		for(int i = 0; i < 8; i++) {
			//Any shifter that's not in count mode should be active at the start of the
			//next scanline and output pixels starting from X=0 (if rendering is enabled)
			if(_spriteShifterList[i] != NesPpu::SpriteShifterDone) {
				uint8_t bit = (1 << (_spriteShifterList[i] & 0x07));
				if(!(_countingSpriteShifters & bit)) {
					_activeSpriteShifters |= bit;
				}
			}
		}

		UpdateProcessSpritesFlag();
	}
}

template<class T> void NesPpu<T>::UpdateProcessSpritesFlag()
{
	_processSprites = _spriteCount || _activeSpriteShifters || _dotSkipped;
}

template<class T> void NesPpu<T>::ProcessSpriteEvaluationStart()
{
	_sprite0Added = false;
	_spriteInRange = false;
	_secondaryOamAddr = 0;

	_overflowBugCounter = 0;

	_oamCopyDone = false;

	//Sprite evaluation does not necessarily start on the first byte of OAM
	//it can start on any byte (based on the OAM address in 2003), and interprets
	//that byte as the "sprite 0" Y coordinate.
	_firstVisibleSpriteAddr = _spriteRamAddr & 0xFC;
	_lastVisibleSpriteAddr = _firstVisibleSpriteAddr;
}

template<class T> void NesPpu<T>::ProcessSpriteShifters()
{
	//Handle sprite shifter counting.
	if(_nextSpriteShifterCycle == _cycle) {
		while((uint32_t)(_spriteShifterList[_nextSpriteShifter] >> 4) == _cycle) {
			uint8_t bit = (1 << (_spriteShifterList[_nextSpriteShifter] & 7));
			if(_countingSpriteShifters & bit) {
				//If shifter was in count mode, turn off count mode,
				//set the expired flag (counting is done) and enable "shift" mode
				_activeSpriteShifters |= bit;
				_expiredSpriteShifters |= bit;
				_countingSpriteShifters &= ~bit;
			}
			_nextSpriteShifter++;
		}
		_nextSpriteShifterCycle = _spriteShifterList[_nextSpriteShifter] >> 4;
	}
}

template<class T> void NesPpu<T>::ProcessSpriteEvaluation()
{
	if(_cycle < 65) {
		//Clear secondary OAM at between cycle 1 and 64
		_oamCopybuffer = 0xFF;
		_secondarySpriteRam[_secondaryOamAddr & 0x1F] = 0xFF;

		_secondaryOamAddr += !(_cycle & 1);
	} else {
		if(_cycle & 0x01) {
			if(_cycle == 65) {
				ProcessSpriteEvaluationStart();
			}

			//Read a byte from the primary OAM on odd cycles
			_oamCopybuffer = ReadSpriteRam(_spriteRamAddr);
		} else {
			uint8_t spriteAddrH = _spriteRamAddr >> 2;
			uint8_t spriteAddrL = _spriteRamAddr & 3;

			//(Not entirely confirmed - but matches observed behavior)
			//For early PPUs (2C02B and earlier), after sprite eval wraps back to the start of OAM,
			//all subsequent sprites appear to be considered as "out of range", causing only their
			//Y coordinate to be copied to secondary OAM, and then skipping to the next sprite.
			//However, if the last Y position copied to secondary OAM by this process happens to be
			//"in range", it will be end up being shown as a sprite. The sprite's remaining 3 bytes
			//will be $FF (because secondary OAM was cleared at the start of the scanline), causing
			//it to display pixels from sprite tile $FF at X=255, with h+v mirroring and sprite palette 3.
			if(_oamCopyDone && !_settings->GetNesConfig().EnablePpuSpriteEvalBug) {
				spriteAddrH = (spriteAddrH + 1) & 0x3F;
				//"As seen above, a side effect of the OAM write disable signal is to turn writes to the secondary OAM into reads from it."
				_oamCopybuffer = _secondarySpriteRam[_secondaryOamAddr & 0x1F];
			} else {
				if(!_spriteInRange && _scanline >= _oamCopybuffer && _scanline < _oamCopybuffer + (_control.LargeSprites ? 16 : 8)) {
					_spriteInRange = !_oamCopyDone;
				}

				if(_secondaryOamAddr < 0x20) {
					//Copy 1 byte to secondary OAM
					_secondarySpriteRam[_secondaryOamAddr] = _oamCopybuffer;

					if(_spriteInRange) {
						if(_cycle == 66) {
							//If the first Y coordinate we load is in range, set the sprite 0 flag
							//This happens even if this isn't actually the first sprite in OAM
							//(i.e because OAMADDR was not 0 when evaluation started)
							_sprite0Added = true;
						}

						spriteAddrL++;
						_secondaryOamAddr++;

						if(spriteAddrL >= 4) {
							spriteAddrH = (spriteAddrH + 1) & 0x3F;
							spriteAddrL = 0;

							if(spriteAddrH == 0) {
								_oamCopyDone = true;
							}
						}

						//Note: Using "(_secondaryOamAddr & 0x03) == 0" instead of "spriteAddrL == 0" is required
						//to replicate a hardware bug noticed in oam_flicker_test_reenable when disabling & re-enabling
						//rendering on a single scanline
						//TODO This should actually be using a timer (which runs even with rendering off) rather than the OAM2 address.
						if((_secondaryOamAddr & 0x03) == 0) {
							//Done copying all 4 bytes
							_spriteInRange = false;
							_lastVisibleSpriteAddr = (spriteAddrH - 1) * 4;

							if(spriteAddrL != 0) {
								//Normally, if the sprite eval started on a non-multiple-of-4 address, it would
								//resync here and start reading the first byte of next entry as the Y value.
								//But if the last byte read/copied, interpreted as a Y coordinate, has a value
								//that makes it fall "in range" with the current scanline, then the lower 2 bits
								//of the address aren't reset, which means the next sprite will also be interpreted incorrectly
								bool inRange = (_scanline >= _oamCopybuffer && _scanline < _oamCopybuffer + (_control.LargeSprites ? 16 : 8));
								if(!inRange) {
									spriteAddrL = 0;
								}
							}
						}
					} else {
						//Nothing to copy, skip to next sprite
						spriteAddrH = (spriteAddrH + 1) & 0x3F;
						spriteAddrL = 0;
						if(spriteAddrH == 0) {
							_oamCopyDone = true;
						}
					}
				} else {
					//"As seen above, a side effect of the OAM write disable signal is to turn writes to the secondary OAM into reads from it."
					_oamCopybuffer = _secondarySpriteRam[_secondaryOamAddr & 0x1F];

					//8 sprites have been found, check next sprite for overflow + emulate PPU bug
					if(_oamCopyDone) {
						//Skip to next sprite (this scenario only happens when the X=255 sprite bug emulation is enabled)
						spriteAddrH = (spriteAddrH + 1) & 0x3F;
						spriteAddrL = 0;
					} else if(_spriteInRange) {
						//Sprite is visible, consider this to be an overflow
						_statusFlags.SpriteOverflow = true;
						spriteAddrL = (spriteAddrL + 1);
						if(spriteAddrL == 4) {
							spriteAddrH = (spriteAddrH + 1) & 0x3F;
							spriteAddrL = 0;
						}

						if(_overflowBugCounter == 0) {
							_overflowBugCounter = 3;
						} else if(_overflowBugCounter > 0) {
							_overflowBugCounter--;
							if(_overflowBugCounter == 0) {
								//"After it finishes "fetching" this sprite(and setting the overflow flag), it realigns back at the beginning of this line and then continues here on the next sprite"
								_oamCopyDone = true;
								spriteAddrL = 0;
							}
						}
					} else {
						//Sprite isn't on this scanline, trigger sprite evaluation bug - increment both H & L at the same time
						spriteAddrH = (spriteAddrH + 1) & 0x3F;
						spriteAddrL = (spriteAddrL + 1) & 0x03;

						if(spriteAddrH == 0) {
							_oamCopyDone = true;
						}
					}
				}
			}
			_spriteRamAddr = (spriteAddrL & 0x03) | (spriteAddrH << 2);
		}
	}
}

template<class T> uint8_t NesPpu<T>::ReadSpriteRam(uint8_t addr)
{
	if(!_enableOamDecay) {
		return _spriteRam[addr];
	} else {
		uint64_t elapsedCycles = _console->GetCpu()->GetCycleCount() - _oamDecayCycles[addr >> 3];
		if(elapsedCycles <= NesPpu<T>::OamDecayCycleCount) {
			_oamDecayCycles[addr >> 3] = _console->GetCpu()->GetCycleCount();
		} else {
			if(_mask.SpritesEnabled) {
				//When debugging with the break on decayed oam read flag turned on, break (only if sprite rendering is enabled to avoid false positives)
				_emu->BreakIfDebugging(CpuType::Nes, BreakSource::NesBreakOnDecayedOamRead);
			}

			//If this 8-byte row hasn't been read/written to in over 3000 cpu cycles (~1.7ms),
			//decay the row (set it to addr, clear the bits that don't exist on some bytes)
			for(int i = 0; i < 8; i++) {
				int sprAddr = (addr & 0xF8) | i;
				_spriteRam[sprAddr] = (sprAddr & 0x03) == 0x02 ? (sprAddr & 0xE3) : sprAddr;
			}
		}
		return _spriteRam[addr];
	}
}

template<class T> void NesPpu<T>::WriteSpriteRam(uint8_t addr, uint8_t value)
{
	_spriteRam[addr] = value;
	if(_enableOamDecay) {
		_oamDecayCycles[addr >> 3] = _console->GetCpu()->GetCycleCount();
	}
}

template<class T> uint16_t* NesPpu<T>::GetScreenBuffer(bool previousBuffer, bool processGrayscaleEmphasisBits)
{
	if(!previousBuffer && processGrayscaleEmphasisBits) {
		UpdateGrayscaleAndIntensifyBits();
	}
	return previousBuffer ? ((_currentOutputBuffer == _outputBuffers[0]) ? _outputBuffers[1] : _outputBuffers[0]) : _currentOutputBuffer;
}

template<class T> void NesPpu<T>::DebugCopyOutputBuffer(uint16_t* target)
{
	memcpy(target, _currentOutputBuffer, NesConstants::ScreenPixelCount * sizeof(uint16_t));
}

template<class T> void NesPpu<T>::SendFrame()
{
	UpdateGrayscaleAndIntensifyBits();

	_emu->ProcessEvent(EventType::EndFrame);

	void* frameData = ((T*)this)->OnBeforeSendFrame();

	if(_console->IsVsMainConsole()) {
		_emu->GetNotificationManager()->SendNotification(ConsoleNotificationType::PpuFrameDone, _currentOutputBuffer);
	}

	//Get phase at the start of the current frame (341*241 cycles ago)
	uint32_t videoPhase = ((_masterClock / _masterClockDivider) - 82181) % 3;
	NesConfig& cfg = _console->GetNesConfig();
	if(_region != ConsoleRegion::Ntsc || cfg.PpuExtraScanlinesAfterNmi != 0 || cfg.PpuExtraScanlinesBeforeNmi != 0) {
		//Force 2-phase pattern for PAL or when overclocking is used
		videoPhase = _frameCount & 0x01;
	}

	RenderedFrame frame(_currentOutputBuffer, NesConstants::ScreenWidth, NesConstants::ScreenHeight, 1.0, _frameCount, _console->GetControlManager()->GetPortStates(), videoPhase);
	frame.Data = frameData; //HD packs

	if(_console->GetVsMainConsole() || _console->GetVsSubConsole()) {
		SendFrameVsDualSystem(frame);
		if(_console->IsVsMainConsole()) {
			_emu->ProcessEndOfFrame();
		}
	} else {
		bool forRewind = _emu->GetRewindManager()->IsRewinding();
		_emu->GetVideoDecoder()->UpdateFrame(frame, forRewind, forRewind);
		_emu->ProcessEndOfFrame();
	}

	_enableOamDecay = _settings->GetNesConfig().EnableOamDecay;
}

template<class T> void NesPpu<T>::SendFrameVsDualSystem(RenderedFrame& frame)
{
	NesConfig& cfg = _settings->GetNesConfig();
	bool forRewind = _emu->GetRewindManager()->IsRewinding();

	if(cfg.VsDualVideoOutput == VsDualOutputOption::MainSystemOnly && _console->IsVsMainConsole()) {
		_emu->GetVideoDecoder()->UpdateFrame(frame, forRewind, forRewind);
	} else if(cfg.VsDualVideoOutput == VsDualOutputOption::SubSystemOnly && !_console->IsVsMainConsole()) {
		_emu->GetVideoDecoder()->UpdateFrame(frame, forRewind, forRewind);
	} else if(cfg.VsDualVideoOutput == VsDualOutputOption::Both) {
		if(_console->IsVsMainConsole()) {
			VideoMergeResult merged = AvMergeUtilities::MergeFrames(frame, ((NesPpu<T>*)_console->GetVsSubConsole()->GetPpu())->_currentOutputBuffer);
			_emu->GetVideoDecoder()->UpdateFrame(merged.Frame, true, forRewind);
		}
	}
}

template<class T> void NesPpu<T>::BeginVBlank()
{
	TriggerNmi();
}

template<class T> void NesPpu<T>::TriggerNmi()
{
	if(_control.NmiOnVerticalBlank) {
		_console->GetCpu()->SetNmiFlag();
	}
}

template<class T> void NesPpu<T>::UpdateApuStatus()
{
	NesApu* apu = _console->GetApu();
	bool enabled = true;
	if(_scanline > 240) {
		if(_scanline > _standardVblankEnd) {
			//Disable APU for extra lines after NMI
			enabled = false;
		} else if(_scanline >= _standardNmiScanline && _scanline < _nmiScanline) {
			//Disable APU for extra lines before NMI
			enabled = false;
		}
	}
	apu->SetApuStatus(enabled);
}

template<class T> void NesPpu<T>::DebugUpdateFrameBuffer(bool toGrayscale)
{
	//Clear output buffer for "Draw partial frame" feature
	if(toGrayscale) {
		for(int i = 0; i < NesConstants::ScreenPixelCount; i++) {
			_currentOutputBuffer[i] &= 0x30;
		}
	} else {
		memset(_currentOutputBuffer, 0, NesConstants::ScreenPixelCount * 2);
	}
}

template<class T> void NesPpu<T>::Exec()
{
	if(_cycle < 340) {
		//Process cycles 1 to 340
		_cycle++;

		if(_scanline < 240) {
			((T*)this)->ProcessScanline();
		} else if(_cycle == 1 && _scanline == _nmiScanline) {
			if(!_preventVblFlag) {
				_statusFlags.VerticalBlank = true;
				BeginVBlank();
			}
			_preventVblFlag = false;
		} else if(_region == ConsoleRegion::Pal && _scanline >= _palSpriteEvalScanline) {
			//"During scanlines 265-310, the PAL PPU does not perform sprite evaluation at all - instead, it simply increments the OAM address register
			//every 2 pixels (at x=2, x=4, x=6, x=8, ..., x=334, x=336, x=338, and x=340, but not at x=0)."
			//This code only runs for dots 1-340, so we don't need to check dot 0 explicitly.
			if(!(_cycle & 1)) {
				_spriteRamAddr++;

				if(_enableOamDecay) {
					_oamDecayCycles[_spriteRamAddr >> 3] = _console->GetCpu()->GetCycleCount();
				}
			}
		}
	} else {
		//Process cycle 0
		ProcessScanlineFirstCycle();
	}

	if(_needStateUpdate) {
		UpdateState();
	}

	_emu->ProcessPpuCycle<CpuType::Nes>();
}

template<class T> void NesPpu<T>::ProcessScanlineFirstCycle()
{
	_cycle = 0;
	if(++_scanline > _vblankEnd) {
		_lastUpdatedPixel = -1;
		_scanline = -1;

		//Force prerender scanline sprite fetches to load the dummy $FF tiles (fixes shaking in Ninja Gaiden 3 stage 1 after beating boss)
		_spriteCount = 0;

		_emu->ProcessEvent(EventType::StartFrame);

		UpdateMinimumDrawCycles();
	}

	UpdateApuStatus();

	if(_scanline == _console->GetNesConfig().InputScanline) {
		_console->GetControlManager()->UpdateControlDevices();
		_console->GetControlManager()->UpdateInputState();
	}

	//Cycle = 0
	if(_scanline < 240) {
		std::sort(_spriteShifterList, _spriteShifterList + 8);
		_nextSpriteShifter = 0;
		_nextSpriteShifterCycle = (_spriteShifterList[0] >> 4);

		if(_scanline == -1) {
			_statusFlags.SpriteOverflow = false;
			_statusFlags.Sprite0Hit = false;
			_allowFullPpuAccess = true;

			//2C02E+ bug where OAM reliably corrupts if the row changes at the start of pre-render. Required for Huge Insect.
			if(IsRenderingEnabled() && !_settings->GetNesConfig().DisableOamAddrBug) {
				CorruptOamRow(_spriteRamAddr >> 3, _secondaryOamAddr & 0x1F);
			}

			//Switch to alternate output buffer (VideoDecoder may still be decoding the last frame buffer)
			_currentOutputBuffer = (_currentOutputBuffer == _outputBuffers[0]) ? _outputBuffers[1] : _outputBuffers[0];
			_emu->AddDebugEvent<CpuType::Nes>(DebugEventType::BgColorChange);
		} else if(_prevRenderingEnabled) {
			_secondaryOamAddr = 0;

			if(_scanline > 0 || (!(_frameCount & 0x01) || _region != ConsoleRegion::Ntsc || GetPpuModel() != PpuModel::Ppu2C02)) {
				//Set bus address to the tile address calculated from the unused NT fetches at the end of the previous scanline
				//This doesn't happen on scanline 0 if the last dot of the previous frame was skipped
				SetBusAddress((_tile.TileAddr << 4) | (_videoRamAddr >> 12) | _control.BackgroundPatternAddr);
			}
		}
	} else if(_scanline == 240) {
		if(_prevRenderingEnabled) {
			_secondaryOamAddr = 0;
		}
		//At the start of vblank, the bus address is set back to VideoRamAddr.
		//According to Visual NES, this occurs on scanline 240, cycle 1, but is done here on cycle 0 for performance reasons
		SetBusAddress(_videoRamAddr & 0x3FFF);
		_emu->AddDebugEvent<CpuType::Nes>(DebugEventType::BgColorChange);
		SendFrame();
		_frameCount++;
	}

	//Refresh the decay timer for the current OAM1 row if in vblank/fblank.
	//This happens every dot on hardware, but doing it once per scanline should be more than enough while reducing performance cost.
	if(_enableOamDecay && (_scanline >= 240 || !IsRenderingEnabled())) {
		_oamDecayCycles[_spriteRamAddr >> 3] = _console->GetCpu()->GetCycleCount();
	}
}

template<class T> void NesPpu<T>::CorruptOamRow(uint8_t sourceRow, uint8_t destRow)
{
	if(sourceRow != destRow && _region != ConsoleRegion::Pal) {
		uint8_t sourceByte = sourceRow << 3;
		uint8_t destByte = destRow << 3;

		memcpy(_spriteRam + destByte, _spriteRam + sourceByte, 8);
		_secondarySpriteRam[destRow] = _secondarySpriteRam[sourceRow];
	}
}

template<class T> void NesPpu<T>::UpdateState()
{
	_needStateUpdate = false;

	//Rendering enabled flag is apparently set with a 1 cycle delay (i.e setting it at cycle 5 will render cycle 6 like cycle 5 and then take the new settings for cycle 7)
	if(_prevRenderingEnabled != _renderingEnabled) {
		_emu->AddDebugEvent<CpuType::Nes>(DebugEventType::BgColorChange);
		_prevRenderingEnabled = _renderingEnabled;
		if(_scanline < 240) {
			if(_settings->GetNesConfig().EnablePpuOamRowCorruption && (_cycle >= 257 || (_cycle & 1) == 0)) {
				if(_renderingEnabled) {
					CorruptOamRow(_spriteRamAddr >> 3, _secondaryOamAddr & 0x1F); //OAM1 row to OAM2 row.
				} else {
					CorruptOamRow(_secondaryOamAddr & 0x1F, _spriteRamAddr >> 3); //OAM2 row to OAM1 row.
				}
			}

			if(!_prevRenderingEnabled) {
				//When rendering is disabled midscreen, set the vram bus back to the value of 'v'
				SetBusAddress(_videoRamAddr & 0x3FFF);

				if(_cycle >= 65 && _cycle <= 256) {
					//Disabling rendering during OAM evaluation will trigger a glitch causing the current address to be incremented by 1
					//The increment can be "delayed" by 1 PPU cycle depending on whether or not rendering is disabled on an even/odd cycle
					//e.g, if rendering is disabled on an even cycle, the following PPU cycle will increment the address by 5 (instead of 4)
					//     if rendering is disabled on an odd cycle, the increment will wait until the next odd cycle (at which point it will be incremented by 1)
					//In practice, there is no way to see the difference, so we just increment by 1 at the end of the next cycle after rendering was disabled
					_spriteRamAddr++;
				}
			}
		}
	}

	if(_renderingEnabled != (_mask.BackgroundEnabled | _mask.SpritesEnabled)) {
		_renderingEnabled = _mask.BackgroundEnabled | _mask.SpritesEnabled;
		_needStateUpdate = true;
	}

	if(_updateVramAddrDelay > 0) {
		_updateVramAddrDelay--;
		if(_updateVramAddrDelay == 0) {
			if(_console->GetNesConfig().EnablePpu2006ScrollGlitch && _scanline < 240 && IsRenderingEnabled()) {
				//When a $2006 address update lands on the Y or X increment, the written value is bugged and is ANDed with the incremented value
				if(_cycle == 257) {
					_videoRamAddr &= _updateVramAddr;
					_emu->BreakIfDebugging(CpuType::Nes, BreakSource::NesBreakOnPpuScrollGlitch);
				} else if(_cycle > 0 && (_cycle & 0x07) == 0 && (_cycle <= 256 || _cycle > 320)) {
					_videoRamAddr = (_updateVramAddr & ~0x41F) | (_videoRamAddr & _updateVramAddr & 0x41F);
					_emu->BreakIfDebugging(CpuType::Nes, BreakSource::NesBreakOnPpuScrollGlitch);
				} else {
					_videoRamAddr = _updateVramAddr;
				}
			} else {
				_videoRamAddr = _updateVramAddr;
			}

			//The glitches updates corrupt both V and T, so set the new value of V back into T
			_tmpVideoRamAddr = _videoRamAddr;

			if(_scanline >= 240 || !IsRenderingEnabled()) {
				//Only set the VRAM address on the bus if the PPU is not rendering
				//More info here: https://forums.nesdev.com/viewtopic.php?p=132145#p132145
				//Trigger bus address change when setting the vram address - needed by MMC3 IRQ counter
				//"4) Should be clocked when A12 changes to 1 via $2006 write"
				SetBusAddress(_videoRamAddr & 0x3FFF);
			}

			_emu->AddDebugEvent<CpuType::Nes>(DebugEventType::BgColorChange);
		} else {
			_needStateUpdate = true;
		}
	}

	if(_ignoreVramRead > 0) {
		_ignoreVramRead--;
		if(_ignoreVramRead > 0) {
			_needStateUpdate = true;
		}
	}

	if(_needVideoRamIncrement) {
		//Delay vram address increment by 1 ppu cycle after a read/write to 2007
		//This allows the full_palette tests to properly display single-pixel glitches
		//that display the "wrong" color on the screen until the increment occurs (matches hardware)
		_needVideoRamIncrement = false;
		UpdateVideoRamAddr();
	}

	if(_ppuMemoryDataReadStateMachine > 0) {
		_ppuMemoryDataReadStateMachine--;
		if(_ppuMemoryDataReadStateMachine == 0) {
			_memoryReadBuffer = ReadVram(_ppuBusAddress & 0x3FFF, MemoryOperationType::Read);
			_needVideoRamIncrement = true;
		}
		_needStateUpdate = true;
	}

	if(_ppuMemoryDataWriteStateMachine > 0) {
		_ppuMemoryDataWriteStateMachine--;
		if(_ppuMemoryDataWriteStateMachine == 0) {
			if((_ppuBusAddress & 0x3FFF) >= 0x3F00) {
				WritePaletteRam(_ppuBusAddress, _ppuMemoryDataWriteLatch);
				_emu->ProcessPpuWrite<CpuType::Nes>(_ppuBusAddress, _ppuMemoryDataWriteLatch, MemoryType::NesPpuMemory);
			} else {
				if(_scanline >= 240 || !IsRenderingEnabled()) {
					_mapper->WriteVram(_ppuBusAddress & 0x3FFF, _ppuMemoryDataWriteLatch);
				} else {
					//During rendering, the value written is ignored, and instead the address' LSB is used (not confirmed, based on Visual NES)
					_mapper->WriteVram(_ppuBusAddress & 0x3FFF, _ppuBusAddress & 0xFF);
					_emu->BreakIfDebugging(CpuType::Nes, BreakSource::NesInvalidVramAccess);
				}
			}
			_needVideoRamIncrement = true;
		}
		_needStateUpdate = true;
	}

	if(_dotSkipped) {
		_dotSkipped--;
		_needStateUpdate = _needStateUpdate || _dotSkipped;
		UpdateProcessSpritesFlag();
	}
}

template<class T> uint32_t NesPpu<T>::GetPixelBrightness(uint8_t x, uint8_t y)
{
	//Used by Zapper, gives a rough approximation of the brightness level of the specific pixel
	uint16_t pixelData = (_currentOutputBuffer[y << 8 | x] & _paletteRamMask) | _intensifyColorBits;
	return NesDefaultVideoFilter::GetDefaultPixelBrightness(pixelData, GetPpuModel());
}

template<class T> void NesPpu<T>::Serialize(Serializer& s)
{
	SVArray(_paletteRam, 0x20);
	SVArray(_spriteRam, 0x100);
	SVArray(_secondarySpriteRam, 0x20);
	SVArray(_openBusDecayStamp, 8);

	SV(_spriteRamAddr);
	SV(_videoRamAddr);
	SV(_xScroll);
	SV(_tmpVideoRamAddr);
	SV(_writeToggle);
	SV(_highBitShift);
	SV(_lowBitShift);
	SV(_control.VerticalWrite);
	SV(_control.SpritePatternAddr);
	SV(_control.BackgroundPatternAddr);
	SV(_control.LargeSprites);
	SV(_control.NmiOnVerticalBlank);
	SV(_mask.Grayscale);
	SV(_mask.BackgroundMask);
	SV(_mask.SpriteMask);
	SV(_mask.BackgroundEnabled);
	SV(_mask.SpritesEnabled);
	SV(_mask.IntensifyRed);
	SV(_mask.IntensifyGreen);
	SV(_mask.IntensifyBlue);
	SV(_paletteRamMask);
	SV(_intensifyColorBits);
	SV(_statusFlags.SpriteOverflow);
	SV(_statusFlags.Sprite0Hit);
	SV(_statusFlags.VerticalBlank);
	SV(_scanline);
	SV(_cycle);
	SV(_frameCount);
	SV(_memoryReadBuffer);
	SV(_region);

	SV(_ppuBusAddress);
	SV(_masterClock);

	if(s.GetFormat() != SerializeFormat::Map) {
		//Hide these entries from the Lua API
		SV(_currentTilePalette);
		SV(_tile.LowByte);
		SV(_tile.HighByte);
		SV(_tile.PaletteOffset);
		SV(_tile.TileAddr);
		SV(_previousTilePalette);

		SV(_spriteIndex);
		SV(_spriteCount);
		SV(_sprite0Added);
		SV(_sprite0Visible);
		SV(_oamCopybuffer);
		SV(_secondaryOamAddr);
		SV(_spriteInRange);
		SV(_prevRenderingEnabled);
		SV(_renderingEnabled);
		SV(_openBus);
		SV(_ignoreVramRead);

		SVArray(_spriteShifterList, 8);
		SV(_nextSpriteShifter);
		SV(_nextSpriteShifterCycle);
		SV(_activeSpriteShifters);
		SV(_countingSpriteShifters);
		SV(_expiredSpriteShifters);
		SV(_dotSkipped);
		SV(_processSprites);

		SV(_oamCopyDone);
		SV(_needStateUpdate);
		SV(_preventVblFlag);
		SV(_needVideoRamIncrement);
		SV(_overflowBugCounter);
		SV(_updateVramAddr);
		SV(_updateVramAddrDelay);

		SV(_allowFullPpuAccess);

		SV(_ppuMemoryDataReadStateMachine);
		SV(_ppuMemoryDataWriteStateMachine);
		SV(_ppuMemoryDataWriteLatch);

		for(int i = 0; i < _spriteCount; i++) {
			SVI(_spriteTiles[i].SpriteX);
			SVI(_spriteTiles[i].LowByte);
			SVI(_spriteTiles[i].HighByte);
			SVI(_spriteTiles[i].PaletteOffset);
			SVI(_spriteTiles[i].BackgroundPriority);
		}
	}

	if(!s.IsSaving()) {
		UpdateTimings(_region);
		UpdateMinimumDrawCycles();
		UpdateGrayscaleAndIntensifyBits();

		for(int i = 0; i < 0x20; i++) {
			//Set oam decay cycle to the current cycle to ensure it doesn't decay when loading a state
			_oamDecayCycles[i] = _console->GetCpu()->GetCycleCount();
		}

		_spriteShifterList[8] = BaseNesPpu::SpriteShifterDone;

		_lastUpdatedPixel = -1;

		UpdateApuStatus();
	}
}

template NesPpu<DefaultNesPpu>::NesPpu(NesConsole* console);
template uint16_t* NesPpu<DefaultNesPpu>::GetScreenBuffer(bool previousBuffer, bool processGrayscaleEmphasisBits);
template void NesPpu<DefaultNesPpu>::Exec();
template uint32_t NesPpu<DefaultNesPpu>::GetPixelBrightness(uint8_t x, uint8_t y);

template NesPpu<NsfPpu>::NesPpu(NesConsole* console);
template uint16_t* NesPpu<NsfPpu>::GetScreenBuffer(bool previousBuffer, bool processGrayscaleEmphasisBits);
template void NesPpu<NsfPpu>::Exec();
template uint32_t NesPpu<NsfPpu>::GetPixelBrightness(uint8_t x, uint8_t y);

template NesPpu<HdNesPpu>::NesPpu(NesConsole* console);
template uint16_t* NesPpu<HdNesPpu>::GetScreenBuffer(bool previousBuffer, bool processGrayscaleEmphasisBits);
template void NesPpu<HdNesPpu>::Exec();
template uint32_t NesPpu<HdNesPpu>::GetPixelBrightness(uint8_t x, uint8_t y);

template NesPpu<HdBuilderPpu>::NesPpu(NesConsole* console);
template uint16_t* NesPpu<HdBuilderPpu>::GetScreenBuffer(bool previousBuffer, bool processGrayscaleEmphasisBits);
template void NesPpu<HdBuilderPpu>::Exec();
template uint32_t NesPpu<HdBuilderPpu>::GetPixelBrightness(uint8_t x, uint8_t y);
