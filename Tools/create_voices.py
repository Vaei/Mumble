# Copyright (c) Jared Taylor
#
# Creates or updates the voices shipped in /Mumble/Voices. The values here are the source of truth:
# edit them and re-run rather than editing the assets.
#
#   UnrealEditor-Cmd.exe <Project>.uproject -run=pythonscript -script="<this file>" -unattended -nopause -nosplash

import unreal

FOLDER = "/Mumble/Voices"

# name: (synth, prosody, timing). Anything left out keeps the struct's default.
VOICES = {
    "Man": (
        dict(pitch_hz=115, tract_size=1.0, open_quotient=0.6, brightness=0.5, breathiness=0.12, creak=0.3),
        dict(pitch_range=3.0, declination=2.5),
        dict(),
    ),
    "Woman": (
        dict(pitch_hz=215, tract_size=0.84, open_quotient=0.68, brightness=0.55, breathiness=0.22, creak=0.15, mouth_openness=0.9, jitter=0.01),
        dict(pitch_range=4.5, expressiveness=1.2),
        dict(),
    ),
    "Child": (
        dict(pitch_hz=290, tract_size=0.7, open_quotient=0.65, brightness=0.6, breathiness=0.18, creak=0.05, articulation=0.3),
        dict(pitch_range=5.5, expressiveness=1.4, randomness=0.8, syllable_length=1.1),
        dict(characters_per_second=36),
    ),
    "OldMan": (
        dict(pitch_hz=125, tract_size=1.03, open_quotient=0.72, brightness=0.35, breathiness=0.35, creak=0.6, jitter=0.03,
             shimmer=0.18, wobble_rate=5.0, wobble_depth=0.45, mouth_openness=0.7, articulation=0.35),
        dict(pitch_range=2.5, syllable_length=1.25),
        dict(characters_per_second=24, comma_pause=0.1, period_pause=0.26, question_pause=0.26, exclamation_pause=0.26, ellipsis_pause=0.5),
    ),
    "OldWoman": (
        dict(pitch_hz=185, tract_size=0.88, open_quotient=0.72, brightness=0.45, breathiness=0.35, creak=0.4, jitter=0.03,
             shimmer=0.15, wobble_rate=5.5, wobble_depth=0.5, mouth_openness=0.8),
        dict(pitch_range=3.5, syllable_length=1.2),
        dict(characters_per_second=26, comma_pause=0.09, period_pause=0.24, question_pause=0.24, exclamation_pause=0.24),
    ),
    "Brute": (
        dict(pitch_hz=88, tract_size=1.15, open_quotient=0.45, skew=3.0, brightness=0.45, subharmonics=0.35, creak=0.5,
             mouth_openness=0.75, articulation=0.45),
        dict(pitch_range=2.0, declination=2.0, expressiveness=0.8),
        dict(characters_per_second=28),
    ),
    "Mutterer": (
        dict(pitch_hz=105, tract_size=1.05, brightness=0.3, breathiness=0.2, mouth_openness=0.35, articulation=0.2),
        dict(pitch_range=1.5, expressiveness=0.6, randomness=0.4),
        dict(),
    ),
    "Lively": (
        dict(pitch_hz=140, tract_size=0.97, brightness=0.6, articulation=0.5),
        dict(pitch_range=6.0, expressiveness=1.6, randomness=0.9),
        dict(characters_per_second=38),
    ),
    "Critter": (
        dict(pitch_hz=420, tract_size=0.55, brightness=0.55, breathiness=0.1, creak=0.0, mouth_openness=0.9, articulation=0.15),
        dict(pitch_range=7.0, expressiveness=1.5, randomness=1.0, syllable_length=0.9),
        dict(characters_per_second=40),
    ),
    "Noble": (
        dict(pitch_hz=190, tract_size=0.86, open_quotient=0.55, brightness=0.55, breathiness=0.12, articulation=0.55),
        dict(pitch_range=2.0, declination=3.0, expressiveness=0.7, randomness=0.3),
        dict(characters_per_second=28),
    ),
    "Monk": (
        dict(pitch_hz=95, tract_size=1.08, brightness=0.35, breathiness=0.2, creak=0.35, articulation=0.35),
        dict(pitch_range=1.5, expressiveness=0.5, randomness=0.25, syllable_length=1.3),
        dict(characters_per_second=24, period_pause=0.3, comma_pause=0.12),
    ),
}


def fill(struct, values):
    for key, value in values.items():
        struct.set_editor_property(key, float(value))
    return struct


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for seed, (name, (synth, prosody, timing)) in enumerate(VOICES.items()):
        path = f"{FOLDER}/{name}"
        # load_asset rather than does_asset_exist, which answers no in a commandlet
        voice = unreal.load_asset(path)
        if voice is None:
            voice = tools.create_asset(name, FOLDER, unreal.MumbleVoice, unreal.DataAssetFactory())
        if voice is None:
            unreal.log_error(f"[Mumble] could not create {path}")
            continue

        voice.set_editor_property("synth", fill(unreal.MumbleVoiceSynth(), synth))
        voice.set_editor_property("prosody", fill(unreal.MumbleProsody(), prosody))
        voice.set_editor_property("timing", fill(unreal.MumbleTiming(), timing))
        voice.set_editor_property("seed", seed * 7919)
        unreal.EditorAssetLibrary.save_loaded_asset(voice, only_if_is_dirty=False)
        unreal.log(f"[Mumble] wrote {path}")


main()
