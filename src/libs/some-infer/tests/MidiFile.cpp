/*
 * Copyright 2003-2012 by David G. Slomin
 * Copyright 2012-2015 Augustin Cavalier <waddlesplash>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "MidiFile.h"

#include <cstdint>
#include <fstream>
#include <map>

MidiEvent::MidiEvent() {
    fTrackNumber = -1;
    fType = Invalid;
    fVoice = -1;
    fNote = -1;
    fVelocity = -1;
    fAmount = -1;
    fNumber = -1;
    fValue = -1;
    fNumerator = -1;
    fDenominator = -1;
    fData = {};
    fTick = -1;
}
MidiEvent::~MidiEvent() = default;

uint32_t MidiEvent::message() const {
    union {
        unsigned char data_as_bytes[4];
        uint32_t data_as_uint32;
    } u{};

    switch (fType) {
    case NoteOff:
        u.data_as_bytes[0] = 0x80 | fVoice;
        u.data_as_bytes[1] = fNote;
        u.data_as_bytes[2] = fVelocity;
        u.data_as_bytes[3] = 0;
        break;

    case NoteOn:
        u.data_as_bytes[0] = 0x90 | fVoice;
        u.data_as_bytes[1] = fNote;
        u.data_as_bytes[2] = fVelocity;
        u.data_as_bytes[3] = 0;
        break;

    case KeyPressure:
        u.data_as_bytes[0] = 0xA0 | fVoice;
        u.data_as_bytes[1] = fNote;
        u.data_as_bytes[2] = fAmount;
        u.data_as_bytes[3] = 0;
        break;

    case ControlChange:
        u.data_as_bytes[0] = 0xB0 | fVoice;
        u.data_as_bytes[1] = fNumber;
        u.data_as_bytes[2] = fValue;
        u.data_as_bytes[3] = 0;
        break;

    case ProgramChange:
        u.data_as_bytes[0] = 0xC0 | fVoice;
        u.data_as_bytes[1] = fNumber;
        u.data_as_bytes[2] = 0;
        u.data_as_bytes[3] = 0;
        break;

    case ChannelPressure:
        u.data_as_bytes[0] = 0xD0 | fVoice;
        u.data_as_bytes[1] = fAmount;
        u.data_as_bytes[2] = 0;
        u.data_as_bytes[3] = 0;
        break;

    case PitchWheel:
        u.data_as_bytes[0] = 0xE0 | fVoice;
        u.data_as_bytes[2] = fValue >> 7;
        u.data_as_bytes[1] = fValue;
        u.data_as_bytes[3] = 0;
        break;

    default:
        return 0;
    }
    return u.data_as_uint32;
}

void MidiEvent::setMessage(const uint32_t data) {
    union {
        uint32_t data_as_uint32;
        unsigned char data_as_bytes[4];
    } u{};

    u.data_as_uint32 = data;

    switch (u.data_as_bytes[0] & 0xF0) {
    case 0x80:
        {
            setType(NoteOff);
            setVoice(u.data_as_bytes[0] & 0x0F);
            setNote(u.data_as_bytes[1]);
            setVelocity(u.data_as_bytes[2]);
            return;
        }
    case 0x90:
        {
            setType(NoteOn);
            setVoice(u.data_as_bytes[0] & 0x0F);
            setNote(u.data_as_bytes[1]);
            setVelocity(u.data_as_bytes[2]);
            return;
        }
    case 0xA0:
        {
            setType(KeyPressure);
            setVoice(u.data_as_bytes[0] & 0x0F);
            setNote(u.data_as_bytes[1]);
            setAmount(u.data_as_bytes[2]);
            return;
        }
    case 0xB0:
        {
            setType(ControlChange);
            setVoice(u.data_as_bytes[0] & 0x0F);
            setNumber(u.data_as_bytes[1]);
            setValue(u.data_as_bytes[2]);
            return;
        }
    case 0xC0:
        {
            setType(ProgramChange);
            setVoice(u.data_as_bytes[0] & 0x0F);
            setNumber(u.data_as_bytes[1]);
            return;
        }
    case 0xD0:
        {
            setType(ChannelPressure);
            setVoice(u.data_as_bytes[0] & 0x0F);
            setAmount(u.data_as_bytes[1]);
            return;
        }
    case 0xE0:
        {
            setType(PitchWheel);
            setVoice(u.data_as_bytes[0] & 0x0F);
            setValue((u.data_as_bytes[2] << 7) | u.data_as_bytes[1]);
            return;
        }
    default:;
    }
}

float MidiEvent::tempo() {
    if ((fType != Meta) || (fNumber != Tempo)) {
        return -1;
    }

    const auto *buffer = reinterpret_cast<unsigned char *>(fData.data());
    const int32_t midi_tempo = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];
    return static_cast<float>(60000000.0 / midi_tempo);
}

/* End of MidiEvent functions, on to QMidiFile */

MidiFile::MidiFile() : fDisableSort(false) { clear(); }
MidiFile::~MidiFile() { clear(); }

void MidiFile::clear() {
    for (const auto *e : fEvents)
        delete e;
    fEvents.clear();
    fTempoEvents.clear();
    fTracks.clear();
    fDivType = PPQ;
    fResolution = 0;
    fFileFormat = 1;
}

MidiFile *MidiFile::oneTrackPerVoice() const {
    if (fFileFormat != 0) {
        return nullptr;
    }

    auto *ret = new MidiFile();

    ret->setDivisionType(fDivType);
    ret->setResolution(fResolution);
    ret->setFileFormat(1);

    std::map<int, int> tracks; // voice -> track

    ret->createTrack(); // Track 0
    ret->fDisableSort = true;

    for (const auto *event : fEvents) {
        auto *e = new MidiEvent();
        *e = *event;

        // Meta event
        if ((e->type() == MidiEvent::Meta) && (e->number() == MidiEvent::TrackName)) {
            e->setTrack(1);
            ret->addEvent(e->tick(), e);
            continue;
        } else if (e->type() == MidiEvent::Meta) {
            e->setTrack(0);
            ret->addEvent(e->tick(), e);
            continue;
        }

        auto it = tracks.find(e->voice());
        if (it == tracks.end()) {
            tracks[e->voice()] = ret->createTrack();
        }

        e->setTrack(tracks[e->voice()]);
        ret->addEvent(e->tick(), e);
    }

    ret->fDisableSort = false;
    ret->sort();

    return ret;
}

bool isGreaterThan(const MidiEvent *e1, const MidiEvent *e2) {
    const int32_t e1t = e1->tick();
    const int32_t e2t = e2->tick();
    return (e1t < e2t);
}
void MidiFile::sort() {
    if (fDisableSort) {
        return;
    }
    std::stable_sort(fEvents.begin(), fEvents.end(), isGreaterThan);
    std::stable_sort(fTempoEvents.begin(), fTempoEvents.end(), isGreaterThan);
}

void MidiFile::addEvent(int32_t tick, MidiEvent *e) {
    e->setTick(tick);
    fEvents.push_back(e);
    if ((e->track() == 0) && (e->type() == MidiEvent::Meta) && (e->number() == MidiEvent::Tempo)) {
        fTempoEvents.push_back(e);
    }
    sort();
}
void MidiFile::removeEvent(MidiEvent *e) {
    fEvents.remove(e);
    if ((e->track() == 0) && (e->type() == MidiEvent::Meta) && (e->number() == MidiEvent::Tempo)) {
        fTempoEvents.remove(e);
    }
}

std::vector<MidiEvent *> MidiFile::eventsForTrack(const int track) const {
    std::vector<MidiEvent *> ret;
    for (auto *e : fEvents) {
        if (e->track() == track) {
            ret.push_back(e);
        }
    }
    return ret;
}

std::vector<MidiEvent *> MidiFile::events(const int voice) const {
    std::vector<MidiEvent *> ret;
    for (auto *e : fEvents) {
        if (e->voice() == voice) {
            ret.push_back(e);
        }
    }
    return ret;
}

int MidiFile::createTrack() {
    const int t = static_cast<int>(fTracks.size());
    fTracks.push_back(t);
    return t;
}

void MidiFile::removeTrack(int track) {
    const auto it = std::find_if(fTracks.begin(), fTracks.end(), [track](const auto &t) { return t == track; });
    if (it != fTracks.end()) {
        fTracks.erase(it);
    }
}

int32_t MidiFile::trackEndTick(int track) {
    for (auto it = fEvents.rbegin(); it != fEvents.rend(); ++it) {
        auto *e = *it;
        if (e->track() == track) {
            return e->tick();
        }
    }
    return 0;
}

MidiEvent *MidiFile::createNote(const int track, const int32_t start_tick, const int32_t end_tick, const int voice,
                                const int note, const int start_velocity, const int end_velocity) {
    createNoteOffEvent(track, end_tick, voice, note, end_velocity);
    return createNoteOnEvent(track, start_tick, voice, note, start_velocity);
}

MidiEvent *MidiFile::createNoteOffEvent(const int track, const int32_t tick, const int voice, const int note,
                                        const int velocity) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::NoteOff);
    e->setTrack(track);
    e->setVoice(voice);
    e->setNote(note);
    e->setVelocity(velocity);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createNoteOnEvent(const int track, const int32_t tick, const int voice, const int note,
                                       const int velocity) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::NoteOn);
    e->setTrack(track);
    e->setVoice(voice);
    e->setNote(note);
    e->setVelocity(velocity);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createKeyPressureEvent(const int track, const int32_t tick, const int voice, const int note,
                                            const int amount) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::KeyPressure);
    e->setTrack(track);
    e->setVoice(voice);
    e->setNote(note);
    e->setAmount(amount);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createChannelPressureEvent(const int track, const int32_t tick, const int voice,
                                                const int amount) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::ChannelPressure);
    e->setTrack(track);
    e->setVoice(voice);
    e->setAmount(amount);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createControlChangeEvent(const int track, const int32_t tick, const int voice, const int number,
                                              const int value) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::ControlChange);
    e->setTrack(track);
    e->setVoice(voice);
    e->setNumber(number);
    e->setValue(value);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createProgramChangeEvent(const int track, const int32_t tick, const int voice, const int number) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::ProgramChange);
    e->setTrack(track);
    e->setVoice(voice);
    e->setNumber(number);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createPitchWheelEvent(const int track, const int32_t tick, const int voice, const int value) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::PitchWheel);
    e->setTrack(track);
    e->setVoice(voice);
    e->setValue(value);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createSysexEvent(const int track, const int32_t tick, const std::vector<char> &data) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::SysEx);
    e->setTrack(track);
    e->setData(data);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createMetaEvent(const int track, const int32_t tick, const int number,
                                     const std::vector<char> &data) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::Meta);
    e->setTrack(track);
    e->setNumber(number);
    e->setData(data);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createTempoEvent(int track, int32_t tick, float tempo) {
    long midi_tempo = 60000000L / tempo;
    std::vector<char> buffer(3, 0);
    buffer[0] = (midi_tempo >> 16) & 0xFF;
    buffer[1] = (midi_tempo >> 8) & 0xFF;
    buffer[2] = midi_tempo & 0xFF;
    return createMetaEvent(track, tick, MidiEvent::Tempo, buffer);
}
MidiEvent *MidiFile::createTimeSignatureEvent(int track, int32_t tick, int numerator, int denominator) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::Meta);
    e->setNumber(MidiEvent::TimeSignature);
    e->setTrack(track);
    e->setNumerator(numerator);
    e->setDenominator(denominator);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createLyricEvent(const int track, const int32_t tick, const std::vector<char> &text) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::Meta);
    e->setNumber(MidiEvent::Lyric);
    e->setTrack(track);
    e->setData(text);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createMarkerEvent(const int track, const int32_t tick, const std::vector<char> &text) {
    auto *e = new MidiEvent();
    e->setType(MidiEvent::Meta);
    e->setNumber(MidiEvent::Marker);
    e->setTrack(track);
    e->setData(text);
    addEvent(tick, e);
    return e;
}
MidiEvent *MidiFile::createVoiceEvent(const int track, const int32_t tick, const uint32_t data) {
    auto *e = new MidiEvent();
    e->setTrack(track);
    e->setMessage(data);
    addEvent(tick, e);
    return e;
}

float MidiFile::timeFromTick(const int32_t tick) const {
    switch (fDivType) {
    case PPQ:
        {
            float tempo_event_time = 0.0;
            int32_t tempo_event_tick = 0;
            float tempo = 120.0;

            for (auto *e : fTempoEvents) {
                if (e->tick() >= tick) {
                    break;
                }
                tempo_event_time += (static_cast<float>(e->tick() - tempo_event_tick) / fResolution / (tempo / 60));
                tempo_event_tick = e->tick();
                tempo = e->tempo();
            }

            float time = tempo_event_time + (static_cast<float>(tick - tempo_event_tick) / fResolution / (tempo / 60));
            return time;
        }
    case SMPTE24:
        return static_cast<float>(tick) / (fResolution * 24.0);
    case SMPTE25:
        return static_cast<float>(tick) / (fResolution * 25.0);
    case SMPTE30DROP:
        return static_cast<float>(tick) / (fResolution * 29.97);
    case SMPTE30:
        return static_cast<float>(tick) / (fResolution * 30.0);
    default:
        return -1;
    }
}

int32_t MidiFile::tickFromTime(const float time) const {
    switch (fDivType) {
    case PPQ:
        {
            float tempo_event_time = 0.0;
            int32_t tempo_event_tick = 0;
            float tempo = 120.0;

            for (auto *e : fTempoEvents) {
                const float next_tempo_event_time =
                    tempo_event_time + (static_cast<float>(e->tick() - tempo_event_tick) / fResolution / (tempo / 60));
                if (next_tempo_event_time >= time)
                    break;
                tempo_event_time = next_tempo_event_time;
                tempo_event_tick = e->tick();
                tempo = e->tempo();
            }

            return tempo_event_tick + static_cast<int32_t>((time - tempo_event_time) * (tempo / 60) * fResolution);
        }
    case SMPTE24:
        return static_cast<int32_t>(time * fResolution * 24.0);
    case SMPTE25:
        return static_cast<int32_t>(time * fResolution * 25.0);
    case SMPTE30DROP:
        return static_cast<int32_t>(time * fResolution * 29.97);
    case SMPTE30:
        return static_cast<int32_t>(time * fResolution * 30.0);
    default:
        return -1;
    }
}

float MidiFile::beatFromTick(const int32_t tick) const {
    switch (fDivType) {
    case PPQ:
        return static_cast<float>(tick) / fResolution;
    case SMPTE24:
        return static_cast<float>(tick) / 24.0;
    case SMPTE25:
        return static_cast<float>(tick) / 25.0;
    case SMPTE30DROP:
        return static_cast<float>(tick) / 29.97;
    case SMPTE30:
        return static_cast<float>(tick) / 30.0;
    default:
        return -1.0;
    }
}

int32_t MidiFile::tickFromBeat(const float beat) const {
    switch (fDivType) {
    case PPQ:
        return static_cast<int32_t>(beat * fResolution);
    case SMPTE24:
        return static_cast<int32_t>(beat * 24.0);
    case SMPTE25:
        return static_cast<int32_t>(beat * 25.0);
    case SMPTE30DROP:
        return static_cast<int32_t>(beat * 29.97);
    case SMPTE30:
        return static_cast<int32_t>(beat * 30);
    default:
        return -1;
    }
}

/*
 * Helpers
 */

int16_t interpret_int16(const unsigned char *buffer) {
    return (static_cast<int16_t>(buffer[0]) << 8) | static_cast<int16_t>(buffer[1]);
}
uint16_t interpret_uint16(const unsigned char *buffer) {
    return (static_cast<uint16_t>(buffer[0]) << 8) | static_cast<uint16_t>(buffer[1]);
}
uint16_t read_uint16(std::ifstream *in) {
    unsigned char buffer[2];
    in->read(reinterpret_cast<char *>(buffer), 2);
    return interpret_uint16(buffer);
}
void write_uint16(std::ofstream *out, const uint16_t value) {
    unsigned char buffer[2];
    buffer[0] = static_cast<unsigned char>((value >> 8) & 0xFF);
    buffer[1] = static_cast<unsigned char>(value & 0xFF);
    out->write(reinterpret_cast<char *>(buffer), 2);
}

uint32_t read_uint32(std::ifstream *in) {
    unsigned char buffer[4];
    in->read(reinterpret_cast<char *>(&buffer), 4);
    return (static_cast<uint32_t>(buffer[0]) << 24) | (static_cast<uint32_t>(buffer[1]) << 16) |
        (static_cast<uint32_t>(buffer[2]) << 8) | static_cast<uint32_t>(buffer[3]);
}
void write_uint32(std::ofstream *out, const uint32_t value) {
    unsigned char buffer[4];
    buffer[0] = static_cast<unsigned char>(value >> 24);
    buffer[1] = static_cast<unsigned char>((value >> 16) & 0xFF);
    buffer[2] = static_cast<unsigned char>((value >> 8) & 0xFF);
    buffer[3] = static_cast<unsigned char>(value & 0xFF);
    out->write(reinterpret_cast<char *>(buffer), 4);
}

uint32_t read_variable_length_quantity(std::ifstream &in) {
    unsigned char b;
    uint32_t value = 0;

    do {
        in.get(reinterpret_cast<char &>(b));
        value = (value << 7) | (b & 0x7F);
    }
    while ((b & 0x80) == 0x80 && in.good());

    return value;
}

void write_variable_length_quantity(std::ofstream &out, uint32_t value) {
    unsigned char buffer[4];
    int offset = 3;

    while (true) {
        buffer[offset] = static_cast<unsigned char>(value & 0x7F);
        if (offset < 3)
            buffer[offset] |= 0x80;
        value >>= 7;

        if ((value == 0) || (offset == 0)) {
            break;
        }
        offset--;
    }
    out.write(reinterpret_cast<char *>(buffer) + offset, 4 - offset);
}

bool MidiFile::load(const std::filesystem::path &filename) {
    clear();

    std::ifstream in(filename, std::ios::binary);
    if (!in.is_open()) {
        return false;
    }

    fDisableSort = true;
    unsigned char chunk_id[4], division_type_and_resolution[4];
    uint32_t chunk_size, chunk_start;
    int file_format, number_of_tracks, number_of_tracks_read = 0;

    in.read(reinterpret_cast<char *>(chunk_id), 4);
    chunk_size = read_uint32(&in);
    chunk_start = in.tellg();

    /* check for the RMID variation on SMF */

    if (memcmp(chunk_id, "RIFF", 4) == 0) {
        in.read(reinterpret_cast<char *>(chunk_id), 4);
        /* technically this one is a type id rather than a chunk id */

        if (memcmp(chunk_id, "RMID", 4) != 0) {
            in.close();
            fDisableSort = false;
            return false;
        }

        in.read(reinterpret_cast<char *>(chunk_id), 4);
        chunk_size = read_uint32(&in);

        if (memcmp(chunk_id, "data", 4) != 0) {
            in.close();
            fDisableSort = false;
            return false;
        }

        in.read(reinterpret_cast<char *>(chunk_id), 4);
        chunk_size = read_uint32(&in);
        chunk_start = in.tellg();
    }

    if (memcmp(chunk_id, "MThd", 4) != 0) {
        in.close();
        fDisableSort = false;
        return false;
    }

    file_format = read_uint16(&in);
    number_of_tracks = read_uint16(&in);
    in.read(reinterpret_cast<char *>(division_type_and_resolution), 2);

    switch (static_cast<signed char>(division_type_and_resolution[0])) {
    case SMPTE24:
        {
            fFileFormat = file_format;
            fDivType = SMPTE24;
            fResolution = division_type_and_resolution[1];
            break;
        }
    case SMPTE25:
        {
            fFileFormat = file_format;
            fDivType = SMPTE25;
            fResolution = division_type_and_resolution[1];
            break;
        }
    case SMPTE30DROP:
        {
            fFileFormat = file_format;
            fDivType = SMPTE30DROP;
            fResolution = division_type_and_resolution[1];
            break;
        }
    case SMPTE30:
        {
            fFileFormat = file_format;
            fDivType = SMPTE30;
            fResolution = division_type_and_resolution[1];
            break;
        }
    default:
        {
            fFileFormat = file_format;
            fDivType = PPQ;
            fResolution = interpret_uint16(division_type_and_resolution);
            break;
        }
    }

    /* forwards compatibility:  skip over any extra header data */
    in.seekg(chunk_start + chunk_size);

    while (number_of_tracks_read < number_of_tracks) {
        in.read(reinterpret_cast<char *>(chunk_id), 4);
        chunk_size = read_uint32(&in);
        chunk_start = in.tellg();

        if (memcmp(chunk_id, "MTrk", 4) == 0) {
            int track = createTrack();
            uint32_t tick, previous_tick = 0;
            int64_t previous_pos = 0;
            unsigned char status, running_status = 0;
            int at_end_of_track = 0;

            while ((in.tellg() < chunk_start + chunk_size) && !at_end_of_track) {
                tick = read_variable_length_quantity(in) + previous_tick;
                previous_tick = tick;

                in.get(reinterpret_cast<char &>(status));

                if ((status & 0x80) == 0x00) {
                    status = running_status;
                    in.seekg(static_cast<int>(in.tellg()) - 1);
                } else {
                    running_status = status;
                }

                if (in.tellg() == previous_pos) {
                    in.close();
                    fDisableSort = false;
                    sort();
                    return false;
                }
                previous_pos = in.tellg();

                switch (status & 0xF0) {
                case 0x80:
                    {
                        int channel = status & 0x0F;
                        char note;
                        in.get(note);
                        char velocity;
                        in.get(velocity);
                        createNoteOffEvent(track, tick, channel, note, velocity);
                        break;
                    }
                case 0x90:
                    {
                        int channel = status & 0x0F;
                        char note;
                        in.get(note);
                        char velocity;
                        in.get(velocity);
                        if (velocity != 0) {
                            createNoteOnEvent(track, tick, channel, note, velocity);
                        } else {
                            createNoteOffEvent(track, tick, channel, note);
                        }
                        break;
                    }
                case 0xA0:
                    {
                        int channel = status & 0x0F;
                        char note;
                        in.get(note);
                        char amount;
                        in.get(amount);
                        createKeyPressureEvent(track, tick, channel, note, amount);
                        break;
                    }
                case 0xB0:
                    {
                        int channel = status & 0x0F;
                        char number;
                        in.get(number);
                        char value;
                        in.get(value);
                        createControlChangeEvent(track, tick, channel, number, value);
                        break;
                    }
                case 0xC0:
                    {
                        int channel = status & 0x0F;
                        char number;
                        in.get(number);
                        createProgramChangeEvent(track, tick, channel, number);
                        break;
                    }
                case 0xD0:
                    {
                        int channel = status & 0x0F;
                        char amount;
                        in.get(amount);
                        createChannelPressureEvent(track, tick, channel, amount);
                        break;
                    }
                case 0xE0:
                    {
                        int channel = status & 0x0F;
                        char value;
                        in.get(value);
                        char value2;
                        in.get(value2);

                        int16_t pitch;
                        pitch = ((value2 & 0x7F) << 7) | (value & 0x7F); // Unpack 14-bit value

                        createPitchWheelEvent(track, tick, channel, pitch);
                        break;
                    }
                case 0xF0:
                    {
                        switch (status) {
                        case 0xF0:
                        case 0xF7:
                            {
                                int data_length = read_variable_length_quantity(in) + 1;
                                std::vector<char> data(1, 0);
                                data[0] = status;

                                std::vector<char> temp(data_length - 1);
                                in.read(temp.data(), data_length - 1);

                                data.insert(data.end(), temp.begin(), temp.end());

                                createSysexEvent(track, tick, data);
                                break;
                            }
                        case 0xFF:
                            {
                                char number;
                                in.get(number);
                                int data_length = read_variable_length_quantity(in);
                                std::vector<char> data(data_length);
                                in.read(data.data(), data_length);

                                if (number == 0x2F) {
                                    at_end_of_track = 1;
                                } else {
                                    createMetaEvent(track, tick, number, data);
                                }
                                break;
                            }
                        default:;
                        }
                        break;
                    }
                default:;
                }
            }

            number_of_tracks_read++;
        } else {
            in.close();
            fDisableSort = false;
            sort();
            return false;
        }

        /* forwards compatibility: skip over any unrecognized chunks, or extra
         * data at the end of tracks. */
        in.seekg(chunk_start + chunk_size);
    }

    in.close();
    fDisableSort = false;
    sort();
    return true;
}

bool MidiFile::save(const std::filesystem::path &filename) {
    if (filename.empty()) {
        return false;
    }

    if (std::filesystem::exists(filename)) {
        std::filesystem::remove(filename);
    }

    std::ofstream out(filename, std::ios::binary);

    if (!out.is_open()) {
        return false;
    }

    out.write("MThd", 4);
    write_uint32(&out, 6);
    write_uint16(&out, static_cast<uint16_t>(fFileFormat));
    write_uint16(&out, static_cast<uint16_t>(fTracks.size()));

    switch (fDivType) {
    case PPQ:
        write_uint16(&out, static_cast<uint16_t>(fResolution));
        break;
    default:
        out.put(fDivType);
        out.put(fResolution);
        break;
    }

    for (const int curTrack : fTracks) {
        out.write("MTrk", 4);

        const int32_t track_size_offset = out.tellp();
        write_uint32(&out, 0);

        const int32_t track_start_offset = out.tellp();

        int32_t previous_tick = 0;

        std::vector<MidiEvent *> eventsForTrk = eventsForTrack(curTrack);
        for (auto *e : eventsForTrk) {
            const int32_t tick = e->tick();
            write_variable_length_quantity(out, tick - previous_tick);

            switch (e->type()) {
            case MidiEvent::NoteOff:
                out.put(0x80 | (e->voice() & 0x0F));
                out.put(e->note() & 0x7F);
                out.put(e->velocity() & 0x7F);
                break;

            case MidiEvent::NoteOn:
                out.put(0x90 | (e->voice() & 0x0F));
                out.put(e->note() & 0x7F);
                out.put(e->velocity() & 0x7F);
                break;

            case MidiEvent::KeyPressure:
                out.put(0xA0 | (e->voice() & 0x0F));
                out.put(e->note() & 0x7F);
                out.put(e->amount() & 0x7F);
                break;

            case MidiEvent::ControlChange:
                out.put(0xB0 | (e->voice() & 0x0F));
                out.put(e->number() & 0x7F);
                out.put(e->value() & 0x7F);
                break;

            case MidiEvent::ProgramChange:
                out.put(0xC0 | (e->voice() & 0x0F));
                out.put(e->number() & 0x7F);
                break;

            case MidiEvent::ChannelPressure:
                out.put(0xD0 | (e->voice() & 0x0F));
                out.put(e->value() & 0x7F);
                break;

            case MidiEvent::PitchWheel:
                {
                    const int value = e->value();
                    out.put(0xE0 | (e->voice() & 0x0F));
                    out.put(value & 0x7F);
                    out.put((value >> 7) & 0x7F);
                    break;
                }
            case MidiEvent::SysEx:
                {
                    const int data_length = e->data().size();
                    const auto data = reinterpret_cast<unsigned char *>(e->data().data());
                    out.put(data[0]);
                    write_variable_length_quantity(out, data_length - 1);
                    out.write(reinterpret_cast<char *>(data) + 1, data_length - 1);
                    break;
                }
            case MidiEvent::Meta:
                {
                    const int data_length = e->data().size();
                    const auto data = reinterpret_cast<unsigned char *>(e->data().data());
                    out.put(0xFF);
                    out.put(e->number() & 0x7F);
                    write_variable_length_quantity(out, data_length);
                    out.write(reinterpret_cast<char *>(data), data_length);
                    break;
                }
            default:
                break;
            }

            previous_tick = tick;
        }

        write_variable_length_quantity(out, trackEndTick(curTrack) - previous_tick);
        out.write("\xFF\x2F\x00", 3);

        const int32_t track_end_offset = out.tellp();

        out.seekp(track_size_offset);
        write_uint32(&out, track_end_offset - track_start_offset);

        out.seekp(track_end_offset);
    }

    out.close();
    return true;
}
