/*
 * Copyright 2003-2012 by David G. Slomin
 * Copyright 2012-2015 Augustin Cavalier <waddlesplash>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "MidiFile.h"
#include <algorithm>

#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>

namespace Midi
{
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
        uint32_t data = 0;
        unsigned char data_as_bytes[4] = {0}; // Initialize the byte array to zero

        switch (fType) {
        case NoteOff:
            data_as_bytes[0] = 0x80 | fVoice;
            data_as_bytes[1] = fNote;
            data_as_bytes[2] = fVelocity;
            break;

        case NoteOn:
            data_as_bytes[0] = 0x90 | fVoice;
            data_as_bytes[1] = fNote;
            data_as_bytes[2] = fVelocity;
            break;

        case KeyPressure:
            data_as_bytes[0] = 0xA0 | fVoice;
            data_as_bytes[1] = fNote;
            data_as_bytes[2] = fAmount;
            break;

        case ControlChange:
            data_as_bytes[0] = 0xB0 | fVoice;
            data_as_bytes[1] = fNumber;
            data_as_bytes[2] = fValue;
            break;

        case ProgramChange:
            data_as_bytes[0] = 0xC0 | fVoice;
            data_as_bytes[1] = fNumber;
            break;

        case ChannelPressure:
            data_as_bytes[0] = 0xD0 | fVoice;
            data_as_bytes[1] = fAmount;
            break;

        case PitchWheel:
            data_as_bytes[0] = 0xE0 | fVoice;
            data_as_bytes[1] = fValue & 0xFF;
            data_as_bytes[2] = fValue >> 7;
            break;

        default:
            return 0;
        }

        // Now we copy the bytes into the uint32_t value
        std::memcpy(&data, data_as_bytes, sizeof(data));

        return data;
    }


    void MidiEvent::setMessage(const uint32_t data) {
        unsigned char data_as_bytes[4];
        std::memcpy(data_as_bytes, &data, sizeof(data_as_bytes));

        switch (data_as_bytes[0] & 0xF0) {
        case 0x80: // NoteOff
            setType(NoteOff);
            setVoice(data_as_bytes[0] & 0x0F);
            setNote(data_as_bytes[1]);
            setVelocity(data_as_bytes[2]);
            break;

        case 0x90: // NoteOn
            setType(NoteOn);
            setVoice(data_as_bytes[0] & 0x0F);
            setNote(data_as_bytes[1]);
            setVelocity(data_as_bytes[2]);
            break;

        case 0xA0: // KeyPressure
            setType(KeyPressure);
            setVoice(data_as_bytes[0] & 0x0F);
            setNote(data_as_bytes[1]);
            setAmount(data_as_bytes[2]);
            break;

        case 0xB0: // ControlChange
            setType(ControlChange);
            setVoice(data_as_bytes[0] & 0x0F);
            setNumber(data_as_bytes[1]);
            setValue(data_as_bytes[2]);
            break;

        case 0xC0: // ProgramChange
            setType(ProgramChange);
            setVoice(data_as_bytes[0] & 0x0F);
            setNumber(data_as_bytes[1]);
            break;

        case 0xD0: // ChannelPressure
            setType(ChannelPressure);
            setVoice(data_as_bytes[0] & 0x0F);
            setAmount(data_as_bytes[1]);
            break;

        case 0xE0: // PitchWheel
            setType(PitchWheel);
            setVoice(data_as_bytes[0] & 0x0F);
            setValue((data_as_bytes[2] << 7) | data_as_bytes[1]);
            break;

        default:
            // Handle unknown/invalid data if necessary
            break;
        }
    }

    float MidiEvent::tempo() const {
        if ((fType != Meta) || (fNumber != Tempo)) {
            return -1;
        }

        // Ensure fData has at least 3 bytes of data for the tempo (MIDI tempo is 3 bytes)
        if (fData.size() < 3) {
            return -1; // Not enough data to extract tempo
        }

        // Use std::memcpy to safely copy the 3 bytes into an integer
        unsigned char buffer[3];
        std::memcpy(buffer, fData.data(), 3); // Assuming fData is a std::vector or similar container

        // Combine the bytes into a 24-bit integer (big-endian format as per MIDI standard)
        const int32_t midi_tempo = (buffer[0] << 16) | (buffer[1] << 8) | buffer[2];

        // Return tempo in BPM (beats per minute)
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

    bool isGreaterThan(const MidiEvent *e1, const MidiEvent *e2) { return e1->tick() < e2->tick(); }

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

            if (auto it = tracks.find(e->voice()); it == tracks.end()) {
                tracks[e->voice()] = ret->createTrack();
            }

            e->setTrack(tracks[e->voice()]);
            ret->addEvent(e->tick(), e);
        }

        ret->fDisableSort = false;
        ret->sort();

        return ret;
    }

    void MidiFile::sort() {
        if (fDisableSort) {
            return;
        }

        // Sort events by tick
        fEvents.sort(isGreaterThan);
        fTempoEvents.sort(isGreaterThan);
    }

    void MidiFile::addEvent(const int32_t tick, MidiEvent *e) {
        e->setTick(tick);
        fEvents.push_back(e);

        // Add tempo events to fTempoEvents
        if ((e->track() == 0) && (e->type() == MidiEvent::Meta) && (e->number() == MidiEvent::Tempo)) {
            fTempoEvents.push_back(e);
        }

        // Sorting events after adding them
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

    void MidiFile::removeTrack(const int track) {
        if (const auto it = std::find(fTracks.begin(), fTracks.end(), track); it != fTracks.end()) {
            fTracks.erase(it);
        }
    }

    int32_t MidiFile::trackEndTick(const int track) {
        for (auto it = fEvents.rbegin(); it != fEvents.rend(); ++it) {
            if (const auto *e = *it; e->track() == track) {
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
    MidiEvent *MidiFile::createControlChangeEvent(const int track, const int32_t tick, const int voice,
                                                  const int number, const int value) {
        auto *e = new MidiEvent();
        e->setType(MidiEvent::ControlChange);
        e->setTrack(track);
        e->setVoice(voice);
        e->setNumber(number);
        e->setValue(value);
        addEvent(tick, e);
        return e;
    }
    MidiEvent *MidiFile::createProgramChangeEvent(const int track, const int32_t tick, const int voice,
                                                  const int number) {
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

    MidiEvent *MidiFile::createTempoEvent(const int track, const int32_t tick, const float tempo) {
        // Convert tempo to MIDI tempo format (microseconds per beat)
        const long midi_tempo = static_cast<long>(60000000.0 / tempo);

        // Use unsigned char to avoid issues with signedness of 'char' across compilers
        std::vector<char> buffer(3, 0);

        // Ensure the buffer contains the 3 bytes of the MIDI tempo message
        buffer[0] = static_cast<char>((midi_tempo >> 16) & 0xFF); // Highest byte
        buffer[1] = static_cast<char>((midi_tempo >> 8) & 0xFF); // Middle byte
        buffer[2] = static_cast<char>(midi_tempo & 0xFF); // Lowest byte

        // Create the meta event with the tempo information
        return createMetaEvent(track, tick, MidiEvent::Tempo, buffer);
    }

    MidiEvent *MidiFile::createTimeSignatureEvent(const int track, const int32_t tick, const int numerator,
                                                  const int denominator) {
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
                    // Accumulate the time for each tempo event
                    tempo_event_time += static_cast<float>(e->tick() - tempo_event_tick) /
                        static_cast<float>(fResolution) * (tempo / 60.0f);
                    tempo_event_tick = e->tick();
                    tempo = e->tempo();
                }

                // Calculate the time from the current tick
                float time = tempo_event_time +
                    static_cast<float>(tick - tempo_event_tick) / static_cast<float>(fResolution) * (tempo / 60.0f);
                return time;
            }
        case SMPTE24:
            return static_cast<float>(tick) / (static_cast<float>(fResolution) * 24.0f);
        case SMPTE25:
            return static_cast<float>(tick) / (static_cast<float>(fResolution) * 25.0f);
        case SMPTE30DROP:
            return static_cast<float>(tick) / (static_cast<float>(fResolution) * 29.97f);
        case SMPTE30:
            return static_cast<float>(tick) / (static_cast<float>(fResolution) * 30.0f);
        default:
            return -1.0f;
        }
    }

    int32_t MidiFile::tickFromTime(const float time) const {
        switch (fDivType) {
        case PPQ:
            {
                float tempo_event_time = 0.0f;
                int32_t tempo_event_tick = 0;
                float tempo = 120.0f;

                for (auto *e : fTempoEvents) {
                    const float next_tempo_event_time = tempo_event_time +
                        (static_cast<float>(e->tick() - tempo_event_tick) / static_cast<float>(fResolution)) *
                            (tempo / 60.0f);
                    if (next_tempo_event_time >= time) {
                        break;
                    }
                    tempo_event_time = next_tempo_event_time;
                    tempo_event_tick = e->tick();
                    tempo = e->tempo();
                }

                return tempo_event_tick +
                    static_cast<int32_t>((time - tempo_event_time) * (tempo / 60.0f) *
                                         static_cast<double>(fResolution));
            }
        case SMPTE24:
            return static_cast<int32_t>(time * static_cast<double>(fResolution) * 24.0);
        case SMPTE25:
            return static_cast<int32_t>(time * static_cast<double>(fResolution) * 25.0);
        case SMPTE30DROP:
            return static_cast<int32_t>(time * static_cast<double>(fResolution) * 29.97);
        case SMPTE30:
            return static_cast<int32_t>(time * static_cast<double>(fResolution) * 30.0);
        default:
            return -1;
        }
    }

    float MidiFile::beatFromTick(const int32_t tick) const {
        switch (fDivType) {
        case PPQ:
            return static_cast<float>(tick) / static_cast<float>(fResolution);
        case SMPTE24:
            return static_cast<float>(tick) / 24.0f;
        case SMPTE25:
            return static_cast<float>(tick) / 25.0f;
        case SMPTE30DROP:
            return static_cast<float>(tick) / 29.97f;
        case SMPTE30:
            return static_cast<float>(tick) / 30.0f;
        default:
            return -1.0f;
        }
    }

    int32_t MidiFile::tickFromBeat(const float beat) const {
        switch (fDivType) {
        case PPQ:
            return static_cast<int32_t>(beat * static_cast<double>(fResolution));
        case SMPTE24:
            return static_cast<int32_t>(beat * 24.0f);
        case SMPTE25:
            return static_cast<int32_t>(beat * 25.0f);
        case SMPTE30DROP:
            return static_cast<int32_t>(beat * 29.97f);
        case SMPTE30:
            return static_cast<int32_t>(beat * 30.0f);
        default:
            return -1;
        }
    }

    /*
     * Helpers
     */

#include <cstdint>
#include <cstring>
#include <fstream>

    // Interpret two bytes as a signed 16-bit integer
    int16_t interpret_int16(const unsigned char *buffer) {
        // Ensure correct sign extension for MSVC and MinGW compatibility
        return static_cast<int16_t>((static_cast<int16_t>(buffer[0]) << 8) | static_cast<int16_t>(buffer[1]));
    }

    // Interpret two bytes as an unsigned 16-bit integer
    uint16_t interpret_uint16(const unsigned char *buffer) {
        // Correct bitwise shifting for unsigned interpretation
        return (static_cast<uint16_t>(buffer[0]) << 8) | static_cast<uint16_t>(buffer[1]);
    }

    // Read a 16-bit unsigned integer from the file stream
    uint16_t read_uint16(std::ifstream *in) {
        unsigned char buffer[2];
        in->read(reinterpret_cast<char *>(buffer), 2);
        // Ensure the read operation was successful
        if (!in->good()) {
            throw std::ios_base::failure("Failed to read uint16");
        }
        return interpret_uint16(buffer);
    }

    // Write a 16-bit unsigned integer to the file stream
    void write_uint16(std::ofstream *out, const uint16_t value) {
        unsigned char buffer[2];
        buffer[0] = static_cast<unsigned char>((value >> 8) & 0xFF); // high byte
        buffer[1] = static_cast<unsigned char>(value & 0xFF); // low byte
        out->write(reinterpret_cast<char *>(buffer), 2);
        // Ensure the write operation was successful
        if (!out->good()) {
            throw std::ios_base::failure("Failed to write uint16");
        }
    }

    // Read a 32-bit unsigned integer from the file stream
    uint32_t read_uint32(std::ifstream *in) {
        unsigned char buffer[4];
        in->read(reinterpret_cast<char *>(buffer), 4);
        // Ensure the read operation was successful
        if (!in->good()) {
            throw std::ios_base::failure("Failed to read uint32");
        }

        // Handle the big-endian byte order correctly
        return (static_cast<uint32_t>(buffer[0]) << 24) | (static_cast<uint32_t>(buffer[1]) << 16) |
            (static_cast<uint32_t>(buffer[2]) << 8) | static_cast<uint32_t>(buffer[3]);
    }

    // Write a 32-bit unsigned integer to the file stream
    void write_uint32(std::ofstream *out, const uint32_t value) {
        unsigned char buffer[4];
        buffer[0] = static_cast<unsigned char>(value >> 24); // highest byte
        buffer[1] = static_cast<unsigned char>((value >> 16) & 0xFF);
        buffer[2] = static_cast<unsigned char>((value >> 8) & 0xFF);
        buffer[3] = static_cast<unsigned char>(value & 0xFF); // lowest byte
        out->write(reinterpret_cast<char *>(buffer), 4);
        // Ensure the write operation was successful
        if (!out->good()) {
            throw std::ios_base::failure("Failed to write uint32");
        }
    }

    // Read a variable-length quantity (VLQ) from the file stream
    uint32_t read_variable_length_quantity(std::ifstream &in) {
        unsigned char b;
        uint32_t value = 0;
        do {
            in.get(reinterpret_cast<char &>(b));
            if (!in.good()) {
                throw std::ios_base::failure("Failed to read VLQ byte");
            }
            value = (value << 7) | (b & 0x7F);
        }
        while ((b & 0x80) == 0x80); // Continue until the MSB is 0

        return value;
    }

    // Write a variable-length quantity (VLQ) to the file stream
    void write_variable_length_quantity(std::ofstream &out, uint32_t value) {
        unsigned char buffer[4];
        int offset = 3;

        while (true) {
            buffer[offset] = static_cast<unsigned char>(value & 0x7F);
            if (offset < 3) {
                buffer[offset] |= 0x80; // Set MSB if there are more bytes
            }
            value >>= 7;

            // Stop when no more bytes to write or offset reaches 0
            if (value == 0 || offset == 0) {
                break;
            }
            offset--;
        }

        out.write(reinterpret_cast<char *>(buffer) + offset, 4 - offset);
        // Ensure the write operation was successful
        if (!out.good()) {
            throw std::ios_base::failure("Failed to write VLQ");
        }
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

        // Check for RMID variation on SMF
        if (memcmp(chunk_id, reinterpret_cast<void const *>("RIFF"), 4) == 0) {
            in.read(reinterpret_cast<char *>(chunk_id), 4);
            if (memcmp(chunk_id, reinterpret_cast<void const *>("RMID"), 4) != 0) {
                in.close();
                fDisableSort = false;
                return false;
            }

            in.read(reinterpret_cast<char *>(chunk_id), 4);
            chunk_size = read_uint32(&in);

            if (memcmp(chunk_id, reinterpret_cast<void const *>("data"), 4) != 0) {
                in.close();
                fDisableSort = false;
                return false;
            }

            in.read(reinterpret_cast<char *>(chunk_id), 4);
            chunk_size = read_uint32(&in);
            chunk_start = in.tellg();
        }

        // Main chunk processing
        if (memcmp(chunk_id, reinterpret_cast<void const *>("MThd"), 4) != 0) {
            in.close();
            fDisableSort = false;
            return false;
        }

        file_format = read_uint16(&in);
        number_of_tracks = read_uint16(&in);
        in.read(reinterpret_cast<char *>(division_type_and_resolution), 2);

        // Interpret time division type
        switch (static_cast<signed char>(division_type_and_resolution[0])) {
        case SMPTE24:
            fFileFormat = file_format;
            fDivType = SMPTE24;
            fResolution = division_type_and_resolution[1];
            break;
        case SMPTE25:
            fFileFormat = file_format;
            fDivType = SMPTE25;
            fResolution = division_type_and_resolution[1];
            break;
        case SMPTE30DROP:
            fFileFormat = file_format;
            fDivType = SMPTE30DROP;
            fResolution = division_type_and_resolution[1];
            break;
        case SMPTE30:
            fFileFormat = file_format;
            fDivType = SMPTE30;
            fResolution = division_type_and_resolution[1];
            break;
        default:
            fFileFormat = file_format;
            fDivType = PPQ;
            fResolution = interpret_uint16(division_type_and_resolution);
            break;
        }

        // Skip over extra header data
        in.seekg(chunk_start + chunk_size);

        while (number_of_tracks_read < number_of_tracks) {
            in.read(reinterpret_cast<char *>(chunk_id), 4);
            chunk_size = read_uint32(&in);
            chunk_start = in.tellg();

            if (memcmp(chunk_id, reinterpret_cast<void const *>("MTrk"), 4) == 0) {
                int track = createTrack();
                int32_t tick, previous_tick = 0;
                int64_t previous_pos = 0;
                unsigned char status, running_status = 0;
                int at_end_of_track = 0;

                while ((in.tellg() < chunk_start + chunk_size) && !at_end_of_track) {
                    tick = static_cast<int32_t>(read_variable_length_quantity(in) + previous_tick);
                    previous_tick = tick;

                    in.get(reinterpret_cast<char &>(status));

                    // Handle running status
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

                    // Handle MIDI events
                    switch (status & 0xF0) {
                    case 0x80:
                        // Note off event
                        {
                            int channel = status & 0x0F;
                            char note, velocity;
                            in.get(note);
                            in.get(velocity);
                            createNoteOffEvent(track, tick, channel, note, velocity);
                            break;
                        }
                    case 0x90:
                        // Note on/off event
                        {
                            int channel = status & 0x0F;
                            char note, velocity;
                            in.get(note);
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
                            pitch =
                                static_cast<int16_t>(((value2 & 0x7F) << 7) | (value & 0x7F)); // Unpack 14-bit value

                            createPitchWheelEvent(track, tick, channel, pitch);
                            break;
                        }
                    case 0xF0:
                        {
                            switch (status) {
                            case 0xF0:
                            case 0xF7:
                                {
                                    int data_length = static_cast<int>(read_variable_length_quantity(in) + 1);
                                    std::vector<char> data(1, 0);
                                    data[0] = static_cast<char>(status);

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
                                    int data_length = static_cast<int>(read_variable_length_quantity(in));
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

        // Remove the existing file if it exists
        if (std::filesystem::exists(filename)) {
            std::filesystem::remove(filename);
        }

        std::ofstream out(filename, std::ios::binary); // Ensure binary mode
        if (!out.is_open()) {
            return false;
        }

        // Write header chunk ("MThd")
        out.write("MThd", 4);
        write_uint32(&out, 6); // Header size
        write_uint16(&out, static_cast<uint16_t>(fFileFormat));
        write_uint16(&out, static_cast<uint16_t>(fTracks.size()));

        // Write division type and resolution
        switch (fDivType) {
        case PPQ:
            write_uint16(&out, static_cast<uint16_t>(fResolution));
            break;
        default:
            out.put(fDivType); // SMPTE types are written as 1 byte
            out.put(static_cast<char>(fResolution)); // Resolution for SMPTE
            break;
        }

        // Write each track chunk
        for (const int curTrack : fTracks) {
            out.write("MTrk", 4); // Track ID

            const auto track_size_offset = static_cast<int32_t>(out.tellp());
            write_uint32(&out, 0); // Placeholder for track size

            const auto track_start_offset = static_cast<int32_t>(out.tellp());

            int32_t previous_tick = 0;

            // Get events for the current track
            std::vector<MidiEvent *> eventsForTrk = eventsForTrack(curTrack);
            for (auto *e : eventsForTrk) {
                const int32_t tick = e->tick();
                write_variable_length_quantity(out, tick - previous_tick); // Delta time

                switch (e->type()) {
                case MidiEvent::NoteOff:
                    out.put(static_cast<char>(0x80 | (e->voice() & 0x0F)));
                    out.put(static_cast<char>(e->note() & 0x7F));
                    out.put(static_cast<char>(e->velocity() & 0x7F));
                    break;

                case MidiEvent::NoteOn:
                    out.put(static_cast<char>(0x90 | (e->voice() & 0x0F)));
                    out.put(static_cast<char>(e->note() & 0x7F));
                    out.put(static_cast<char>(e->velocity() & 0x7F));
                    break;

                case MidiEvent::KeyPressure:
                    out.put(static_cast<char>(0xA0 | (e->voice() & 0x0F)));
                    out.put(static_cast<char>(e->note() & 0x7F));
                    out.put(static_cast<char>(e->amount() & 0x7F));
                    break;

                case MidiEvent::ControlChange:
                    out.put(static_cast<char>(0xB0 | (e->voice() & 0x0F)));
                    out.put(static_cast<char>(e->number() & 0x7F));
                    out.put(static_cast<char>(e->value() & 0x7F));
                    break;

                case MidiEvent::ProgramChange:
                    out.put(static_cast<char>(0xC0 | (e->voice() & 0x0F)));
                    out.put(static_cast<char>(e->number() & 0x7F));
                    break;

                case MidiEvent::ChannelPressure:
                    out.put(static_cast<char>(0xD0 | (e->voice() & 0x0F)));
                    out.put(static_cast<char>(e->value() & 0x7F));
                    break;

                case MidiEvent::PitchWheel:
                    {
                        const int value = e->value();
                        out.put(static_cast<char>(0xE0 | (e->voice() & 0x0F)));
                        out.put(static_cast<char>(value & 0x7F));
                        out.put(static_cast<char>((value >> 7) & 0x7F));
                        break;
                    }
                case MidiEvent::SysEx:
                    {
                        const int data_length = static_cast<int>(e->data().size());
                        const std::vector<char> &dataVec = e->data();
                        const auto data = dataVec.data();
                        out.put(static_cast<char>(data[0]));
                        write_variable_length_quantity(out, data_length - 1);
                        out.write(reinterpret_cast<const char *>(data) + 1, data_length - 1);
                        break;
                    }
                case MidiEvent::Meta:
                    {
                        const int data_length = static_cast<int>(e->data().size());
                        const std::vector<char> &dataVec = e->data();
                        const auto data = dataVec.data();
                        out.put(static_cast<char>(0xFF)); // Meta event type
                        out.put(static_cast<char>(e->number() & 0x7F));
                        write_variable_length_quantity(out, data_length);
                        out.write(data, data_length);
                        break;
                    }
                default:
                    break;
                }

                previous_tick = tick;
            }

            // End of track event
            write_variable_length_quantity(out, trackEndTick(curTrack) - previous_tick);
            out.write("\xFF\x2F\x00", 3); // End of track meta event

            const int32_t track_end_offset = static_cast<int32_t>(out.tellp());

            // Update track size field
            out.seekp(track_size_offset);
            write_uint32(&out, track_end_offset - track_start_offset);

            out.seekp(track_end_offset); // Return to the end of the track
        }

        out.close();
        return true;
    }
} // namespace Midi
