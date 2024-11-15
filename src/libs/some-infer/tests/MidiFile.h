/*
 * Copyright 2003-2012 by David G. Slomin
 * Copyright 2012-2015 Augustin Cavalier <waddlesplash>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <list>
#include <vector>

namespace Midi
{
    class MidiEvent {
    public:
        enum EventType {
            Invalid = -1,
            NoteOn,
            NoteOff,
            KeyPressure,
            ChannelPressure,
            ControlChange,
            ProgramChange,
            PitchWheel,
            Meta,
            SysEx
        };
        enum MetaNumbers {
            /* These types match the MIDI values for them.
             * DON'T CHANGE OR YOU WON'T BE ABLE TO READ/WRITE FILES! */
            TrackName = 0x03,
            Tempo = 0x51,
            TimeSignature = 0x58,
            Lyric = 0x5,
            Marker = 0x6
        };

        MidiEvent();
        ~MidiEvent();

        inline EventType type() const { return fType; }
        inline void setType(const EventType newType) { fType = newType; }

        inline int32_t tick() const { return fTick; }
        inline void setTick(const int32_t tick) { fTick = tick; }
        /* you MUST run the QMidiFile's sort() function after changing ticks! */
        /* otherwise, it will not play or write the file properly! */

        inline int track() const { return fTrackNumber; }
        inline void setTrack(const int trackNumber) { fTrackNumber = trackNumber; }

        inline int voice() const { return fVoice; }
        inline void setVoice(const int voice) { fVoice = voice; }

        inline int note() const { return fNote; }
        inline void setNote(const int note) { fNote = note; }

        inline int velocity() const { return fVelocity; }
        inline void setVelocity(const int velocity) { fVelocity = velocity; }

        inline int amount() const { return fAmount; }
        inline void setAmount(const int amount) { fAmount = amount; }

        inline int number() const { return fNumber; }
        inline void setNumber(const int number) { fNumber = number; }

        inline int value() const { return fValue; }
        inline void setValue(const int value) { fValue = value; }

        float tempo() const;

        inline int numerator() const { return fNumerator; }
        inline void setNumerator(const int numerator) { fNumerator = numerator; }

        inline int denominator() const { return fDenominator; }
        inline void setDenominator(const int denominator) { fDenominator = denominator; }

        inline std::vector<char> data() const { return fData; }
        inline void setData(const std::vector<char> &data) { fData = data; }

        uint32_t message() const;
        void setMessage(uint32_t data);

        inline bool isNoteEvent() const { return ((fType == NoteOn) || (fType == NoteOff)); }

    private:
        int fVoice;
        int fNote;
        int fVelocity;
        int fAmount; // KeyPressure, ChannelPressure
        int fNumber; // ControlChange, ProgramChange, Meta
        int fValue; // PitchWheel, ControlChange
        int fNumerator; // TimeSignature
        int fDenominator; // TimeSignature
        std::vector<char> fData; // Meta, SysEx

        int32_t fTick;
        EventType fType;
        int fTrackNumber;
    };

    class MidiFile {
    public:
        enum DivisionType {
            /* These types match the MIDI values for them.
             * DON'T CHANGE OR YOU WON'T BE ABLE TO READ/WRITE FILES! */
            Invalid = -1,
            PPQ = 0,
            SMPTE24 = -24,
            SMPTE25 = -25,
            SMPTE30DROP = -29,
            SMPTE30 = -30
        };

        MidiFile();
        ~MidiFile();

        void clear();
        bool load(const std::filesystem::path &filename);
        bool save(const std::filesystem::path &filename);

        MidiFile *oneTrackPerVoice() const;

        void sort();

        inline void setFileFormat(const int fileFormat) { fFileFormat = fileFormat; }
        inline int fileFormat() const { return fFileFormat; }

        inline void setResolution(const int resolution) { fResolution = resolution; }
        inline int resolution() const { return fResolution; }

        inline void setDivisionType(const DivisionType type) { fDivType = type; }
        inline DivisionType divisionType() const { return fDivType; }

        void addEvent(int32_t tick, MidiEvent *e);
        void removeEvent(MidiEvent *e);

        int createTrack();
        void removeTrack(int track);
        int32_t trackEndTick(int track);
        inline std::list<int> tracks() { return fTracks; }

        MidiEvent *createNoteOnEvent(int track, int32_t tick, int voice, int note, int velocity);
        MidiEvent *createNoteOffEvent(int track, int32_t tick, int voice, int note, int velocity = 64);
        /* velocity on NoteOff events is how fast to stop the note (127=fastest) */

        MidiEvent *createNote(int track, int32_t start_tick, int32_t end_tick, int voice, int note, int start_velocity,
                              int end_velocity);
        /* returns the start event */

        MidiEvent *createKeyPressureEvent(int track, int32_t tick, int voice, int note, int amount);
        MidiEvent *createChannelPressureEvent(int track, int32_t tick, int voice, int amount);
        MidiEvent *createControlChangeEvent(int track, int32_t tick, int voice, int number, int value);
        MidiEvent *createProgramChangeEvent(int track, int32_t tick, int voice, int number);
        MidiEvent *createPitchWheelEvent(int track, int32_t tick, int voice, int value);
        MidiEvent *createSysexEvent(int track, int32_t tick, const std::vector<char> &data);
        MidiEvent *createMetaEvent(int track, int32_t tick, int number, const std::vector<char> &data);
        MidiEvent *createTempoEvent(int track, int32_t tick, float tempo); /* tempo is in BPM */
        MidiEvent *createTimeSignatureEvent(int track, int32_t tick, int numerator, int denominator);
        MidiEvent *createLyricEvent(int track, int32_t tick, const std::vector<char> &text);
        MidiEvent *createMarkerEvent(int track, int32_t tick, const std::vector<char> &text);
        MidiEvent *createVoiceEvent(int track, int32_t tick, uint32_t data);

        inline std::list<MidiEvent *> events() { return fEvents; }
        std::vector<MidiEvent *> events(int voice) const;
        std::vector<MidiEvent *> eventsForTrack(int track) const;

        float timeFromTick(int32_t tick) const; /* time is in seconds */
        int32_t tickFromTime(float time) const;
        float beatFromTick(int32_t tick) const;
        int32_t tickFromBeat(float beat) const;

    private:
        std::list<MidiEvent *> fEvents;
        std::list<MidiEvent *> fTempoEvents;
        std::list<int> fTracks;
        DivisionType fDivType = PPQ;
        int fResolution{};
        int fFileFormat{};

        bool fDisableSort;
    };
} // namespace Midi
