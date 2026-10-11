#pragma once

#include <inttypes.h>
#include <vector>

#define WRITE_DELAY 2
#define LATCH_DELAY 1
#define READ_DELAY  0

class CounterUnit
{
    friend class TestOfCounterUnit;

    int latch_value;
    int write_state;
    int latch_mode;
    int out;
    int value;
    int mode_int;

    uint8_t write_lsb;
    uint8_t write_msb;
    uint16_t loadvalue;

    union {
        uint32_t flags;
        struct {
            bool armed:1;
            bool load:1;
            bool enabled:1;
            bool bcd:1;
        };
    };

    int delay;

public:
    /* Full deterministic init: latch_mode/out/write_lsb/write_msb were
     * left indeterminate, so a freshly reset chip (and every restore,
     * which resets first) started with garbage in write_value() and
     * read_value() - which then mis-handled the lsb/msb staging of the
     * frame-sync counter games like Exolon rely on. */
    CounterUnit() : latch_value(-1), write_state(0), latch_mode(0), out(0),
        value(0), mode_int(0), write_lsb(0), write_msb(0),
        loadvalue(0), flags(0), delay(0)
    {

    }

    void SetMode(int new_mode, int new_latch_mode, int new_bcd_mode) 
    {
        this->Count(LATCH_DELAY);
        this->delay = LATCH_DELAY;

        this->bcd = new_bcd_mode;
        if ((new_mode & 0x03) == 2) {
            this->mode_int = 2;
        } else if ((new_mode & 0x03) == 3) {
            this->mode_int = 3;
        } else {
            this->mode_int = new_mode;
        }

        switch(this->mode_int) {
            case 0:
                this->out = 0;
                this->armed = true;
                this->enabled = false;
                break;
            case 1:
                this->out = 1;
                this->armed = true;
                this->enabled = false;
                break;
            case 2:
                this->out = 1;
                this->enabled = false;
                // armed?
                break;
            default:
                this->out = 1;
                this->enabled = false;
                // armed?
                break;
        }
        this->load = false;
        this->latch_mode = new_latch_mode;
        this->write_state = 0;
    }

    void Latch(uint8_t w8) {
        this->Count(LATCH_DELAY);
        this->delay = LATCH_DELAY;
        this->latch_value = this->value;
    }

    int Count(int incycles) 
    {
        int cycles = incycles;
        if (this->delay) {
            --this->delay;
            cycles = 0;
        }
        if (!cycles) return this->out;
        if (!this->enabled && !this->load) return this->out;
        int result = this->out;

        switch (this->mode_int) {
            case 0: // Interrupt on terminal count
                if (this->load) {
                    this->value = this->loadvalue;
                    this->enabled = true;
                    this->armed = true;
                    this->out = 0; 
                    result = 0;
                }
                if (this->enabled) {
                    int previous = this->value;
                    this->value -= cycles;
                    if (this->value <= 0) {
                        if (this->armed) {
                            if (previous != 0) this->out = 1;
                            result = -this->value + 1;
                            this->armed = false;
                        }
                        this->value += this->bcd ? 10000 : 65536;
                    }
                }
                break;
            case 1: // Programmable one-shot
                if (!this->enabled && this->load) {
                    //this->value = this->loadvalue; -- quirk!
                    this->enabled = true;
                }
                if (this->enabled) {
                    this->value -= cycles;
                    if (this->value <= 0) {
                        int reload = this->loadvalue == 0 ? 
                            (this->bcd ? 10000 : 0x10000 ) : (this->loadvalue + 1);
                        this->value += reload;
                        //this->value += this->loadvalue + 1;
                    }
                }
                break;
            case 2: // Rate generator
                if (!this->enabled && this->load) {
                    this->value = this->loadvalue;
                    this->enabled = true;
                }
                if (this->enabled) {
                    this->value -= cycles;
                    if (this->value <= 0) {
                        int reload = this->loadvalue == 0 ? 
                            (this->bcd ? 10000 : 0x10000 ) : this->loadvalue;
                        this->value += reload;
                        //this->value += this->loadvalue;
                    }
                }
                // out will go low for one clock pulse but in our machine it should not be 
                // audible
                break;
            case 3: // Square wave generator
                if (!this->enabled && this->load) {
                    this->value = this->loadvalue;
                    this->enabled = true;
                }
                if (this->enabled) {
                    this->value -= 
                        ((this->value == this->loadvalue) && ((this->value&1) == 1)) ? 
                        this->out == 0 ? 3 : 1 : 2; 
                    if (this->value <= 0) {
                        this->out ^= 1;

                        int reload = (this->loadvalue == 0) ? 
                            (this->bcd ? 10000 : 0x10000) : this->loadvalue;
                        this->value += reload;
                        //this->value = this->loadvalue;
                    }
                }
                result = this->out;
                break;
            case 4: // Software triggered strobe
                break;
            case 5: // Hardware triggered strobe
                break;
            default:
                break;
        }

        this->load = false;
        return result;
    }

    void write_value(uint8_t w8) {
        if (this->latch_mode == 3) {
            // lsb, msb             
            switch (this->write_state) {
                case 0:
                    this->write_lsb = w8;
                    this->write_state = 1;
                    break;
                case 1:
                    this->write_msb = w8;
                    this->write_state = 0;
                    this->loadvalue = ((this->write_msb << 8) & 0xffff) | 
                        (this->write_lsb & 0xff);
                    this->load = true;
                    break;
                default:
                    break;
            }
        } else if (this->latch_mode == 1) {
            // lsb only
            this->loadvalue = w8;
            this->load = true;
        } else if (this->latch_mode == 2) {
            // msb only 
            this->value = w8 << 8;
            this->value &= 0xffff;
            this->loadvalue = this->value;
            this->load = true;
        }
        if (this->load) {
            if (this->bcd) {
                this->loadvalue = frombcd(this->loadvalue);
            }
            // I'm deeply sorry about the following part
            switch (this->mode_int) {
                case 0:
                    this->delay = 3; break; 
                case 1:
                    if (!this->enabled) {
                        this->delay = 3; 
                    } 
                    break;
                case 2:
                    if (!this->enabled) {
                        this->delay = 3; 
                    }
                    break;
                case 3:
                    if (!this->enabled) {
                        this->delay = 3; 
                    }
                    break;
                default:
                    this->delay = 4; 
                    break;
            }
        }
    }

    static uint16_t tobcd(uint16_t x) {
        int result = 0;
        for (int i = 0; i < 4; ++i) {
            result |= (x % 10) << (i * 4);
            x /= 10;
        }
        return result;
    }

    static uint16_t frombcd(uint16_t x) {
        int result = 0;
        for (int i = 0; i < 4; ++i) {
            int digit = (x & 0xf000) >> 12;
            if (digit > 9) digit = 9;
            result = result * 10 + digit;
            x <<= 4;
        }
        return result;
    }

    int read_value()
    {
        int value = 0;
        switch (this->latch_mode) {
            case 0:
                // impossibru
                break;
            case 1:
                value = this->latch_value != -1 ? this->latch_value : this->value;
                this->latch_value = -1; 
                value = this->bcd ? tobcd(value) : value;
                value &= 0xff;
                break;
            case 2:
                value = this->latch_value != -1 ? this->latch_value : this->value;
                this->latch_value = -1; 
                value = this->bcd ? tobcd(value) : value;
                value = (value >> 8) & 0xff;
                break;
            case 3:
                value = this->latch_value != -1 ? this->latch_value : this->value;
                value = this->bcd ? tobcd(value) : value;
                switch(this->write_state) {
                    case 0:
                        this->write_state = 1;
                        value = value & 0xff;
                        break;
                    case 1:
                        this->latch_value = -1;
                        this->write_state = 0;
                        value = (value >> 8) & 0xff;
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
        return value;
    }

    /* Save/restore the full counter so a snapshot can carry the live
     * countdown. Each field is written as a fixed 32-bit little-endian
     * word so the stream is independent of host int width. Named dump/
     * undump because `load` is already a counter flag (union bitfield). */
    void dump(std::vector<uint8_t> & to) const
    {
        auto push32 = [&to](int32_t v) {
            to.push_back((uint8_t)(v & 0xff));
            to.push_back((uint8_t)((v >> 8) & 0xff));
            to.push_back((uint8_t)((v >> 16) & 0xff));
            to.push_back((uint8_t)((v >> 24) & 0xff));
        };
        push32(latch_value);
        push32(write_state);
        push32(latch_mode);
        push32(out);
        push32(value);
        push32(mode_int);
        push32((int32_t)write_lsb);
        push32((int32_t)write_msb);
        push32((int32_t)loadvalue);
        push32((int32_t)flags);
        push32(delay);
    }

    void undump(std::vector<uint8_t>::iterator & it)
    {
        auto pop32 = [&it]() -> int32_t {
            int32_t v = (int32_t)((uint8_t)it[0] | ((uint8_t)it[1] << 8) |
                ((uint8_t)it[2] << 16) | ((uint32_t)(uint8_t)it[3] << 24));
            it += 4;
            return v;
        };
        latch_value = pop32();
        write_state = pop32();
        latch_mode = pop32();
        out = pop32();
        value = pop32();
        mode_int = pop32();
        write_lsb = (uint8_t)pop32();
        write_msb = (uint8_t)pop32();
        loadvalue = (uint16_t)pop32();
        flags = (uint32_t)pop32();
        delay = pop32();
    }

    /* Read-only peek for diagnostics; no side effects (see I8253::
     * counter_value et al.). */
    int peek_value() const { return this->value; }
    int peek_loadvalue() const { return this->loadvalue; }
    int peek_enabled() const { return this->enabled ? 1 : 0; }
    int peek_mode() const { return this->mode_int; }
    int peek_write_state() const { return this->write_state; }
};


class I8253
{
private:
    CounterUnit counters[3];
    uint8_t control_word;

public:
    I8253() : control_word(0) {}

    void write_cw(uint8_t w8) 
    {
        unsigned counter_set = (w8 >> 6) & 3;
        int mode_set = (w8 >> 1) & 3;
        int latch_set = (w8 >> 4) & 3;
        int bcd_set = (w8 & 1);

        if ((unsigned)counter_set >= sizeof(counters)/sizeof(counters[0])) {
            // error
            return;
        }

        CounterUnit & ctr = this->counters[counter_set];
        if (latch_set == 0) {
            ctr.Latch(latch_set);
        } else {
            ctr.SetMode(mode_set, latch_set, bcd_set);
        }
    }

    void write(int addr, uint8_t w8) 
    {
        switch (addr & 3) {
            case 0x03:
                return this->write_cw(w8);
            default:
                return this->counters[addr & 3].write_value(w8);
        }
    }

    int read(int addr)
    {
        switch (addr & 3) {
            case 0x03:
                return this->control_word;
            default:
                return this->counters[addr & 3].read_value();
        }
    }

    int Count(int cycles)
    {
        return this->counters[0].Count(cycles) +
            this->counters[1].Count(cycles) +
            this->counters[2].Count(cycles);
    }

    void Count(int cycles, int & c0, int & c1, int & c2)
    {
        c0 = this->counters[0].Count(cycles);
        c1 = this->counters[1].Count(cycles);
        c2 = this->counters[2].Count(cycles);
    }

    void reset()
    {
        counters[0] = CounterUnit();
        counters[1] = CounterUnit();
        counters[2] = CounterUnit();
    }

    /* Advance all counters by n single clocks. The emulation loop calls
     * this once per CPU instruction with the instruction's T-states / 2
     * (one КР580ВИ53 clock = 2 i8080 T-states = 8 of the 12 MHz v_cycles;
     * a 59904-T-state frame is therefore exactly 29952 clocks). Stepping
     * one clock at a time keeps CounterUnit's write/latch `delay` semantics
     * honest, which a single Count(n) would collapse. */
    void tick(int n)
    {
        while (n-- > 0) this->Count(1);
    }

    void dump(std::vector<uint8_t> & to) const
    {
        to.push_back(control_word);
        for (unsigned i = 0; i < sizeof(counters)/sizeof(counters[0]); ++i)
            counters[i].dump(to);
    }

    void undump(std::vector<uint8_t>::iterator & it)
    {
        control_word = (uint8_t)*it++;
        for (unsigned i = 0; i < sizeof(counters)/sizeof(counters[0]); ++i)
            counters[i].undump(it);
    }

    /* Read-only peek accessors for diagnostics (save/restore self-tests,
     * debugger). They have no side effects, unlike read()/read_value()
     * which advances the lsb/msb staging. */
    int counter_value(int n) const { return counters[n].peek_value(); }
    int counter_loadvalue(int n) const { return counters[n].peek_loadvalue(); }
    int counter_enabled(int n) const { return counters[n].peek_enabled(); }
    int counter_mode(int n) const { return counters[n].peek_mode(); }
    int counter_write_state(int n) const { return counters[n].peek_write_state(); }
};

class TimerWrapper
{
private:
    I8253 & timer;
    int sound;
    int average_count;
    int last_sound;
public:
    TimerWrapper(I8253 & _timer) : timer(_timer),
        sound(0), average_count(0), last_sound(0)
    {
    }

    // this is non-canon, but it's very useful when emulator resets timer
    void reset()
    {
        //timer.write(3, 0x36);
        //timer.write(3, 0x76);
        //timer.write(3, 0xb6);
        timer.reset();
    }

    // step with enable per chan?
    //
    int step(int cycles)
    {
        this->last_sound = this->timer.Count(cycles) / cycles;
        return this->last_sound;
    }

    int singlestep(int ena_ch0, int ena_ch1, int ena_ch2)
    {
        int ch0, ch1, ch2;
        this->timer.Count(1, ch0, ch1, ch2);
        this->last_sound = ch0 * ena_ch0 + ch1 * ena_ch1 + ch2 * ena_ch2;
        return this->last_sound;
    }
};
