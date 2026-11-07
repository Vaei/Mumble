# Mumble <img align="right" width=128 height=128 src="Resources/Icon128.png">

Characters mumble along with their dialogue as it types out: a voice synthesised from the text in a MetaSound, with no recorded audio. Each character's voice is a data asset.

To hear every shipped voice, run the `Mumble.Render` automation test (**Tools > Session Frontend > Automation**) and open the WAVs it writes to `Saved/Mumble/`.

UE5.8+

## Get it working

1. Enable the **Mumble** plugin. It needs **Metasound**, which it enables.
2. Add a **Mumble** component to the character who speaks (it is an audio component: give it an attenuation for a voice in the world, none for a voice in the UI).
3. Set its **Voice** to one of the voices in `/Mumble/Voices`: `Man`, `Woman`, `Child`, `OldMan`, `OldWoman`, `Brute`, `Mutterer`, `Lively`, `Critter`, `Noble`, `Monk`.
4. Make it talk, in one of the two ways below. The character mumbles as each line types out.

**Mumble types the line.** Call `Speak(Text)`. The voice's **Timing** paces it, and `OnCharacterRevealed` reports each character so a text box can follow. `Skip()` finishes it at once.

**Your typewriter types the line** (Folio, UMG, subtitles). Call these as it types:

```cpp
Voice->BeginLine(PlainText);     // the whole line, markup removed
Voice->RevealTo(CharIndex);      // as characters appear; index counts code points
Voice->EndLine(bSkipped);        // when it finishes, or true to cut the voice off
```

### With Folio

Bind the document's reveal progress and forward each phase to the speaker's component. Mumble does not depend on Folio; this code lives in your game.

```cpp
// When the dialogue widget activates
Document->OnTextRevealProgressNative.AddUObject(this, &ThisClass::HandleTextRevealProgress);

// When the speaker changes
Voice = NewSpeaker ? NewSpeaker->FindComponentByClass<UMumbleComponent>() : nullptr;

void UMyDialogueWidget::HandleTextRevealProgress(const FFolioTextReveal& Reveal)
{
	UMumbleComponent* Speaker = Voice.Get();
	if (!Speaker || Reveal.ElementId != LineElementId)
	{
		return;
	}

	switch (Reveal.Phase)
	{
	case EFolioTextRevealPhase::Started:
		Speaker->BeginLine(Reveal.Text);
		break;
	case EFolioTextRevealPhase::Revealed:
		Speaker->RevealTo(Reveal.LastChar);
		break;
	case EFolioTextRevealPhase::Finished:
		Speaker->RevealTo(Reveal.LastChar);
		Speaker->EndLine(false);
		break;
	case EFolioTextRevealPhase::Skipped:
		Speaker->EndLine(true);
		break;
	}
}
```

To stop the line's `reveal-sound` clicking over a voiced speaker, put a class on the `<text>` when the speaker has a voice and turn the sound off for it:

```css
.line.voiced { reveal-sound: none; }
```

## Troubleshooting

- **Silent, with "has nothing to speak with" in the log.** No MetaSound could be built or loaded. Check that the Metasound plugin is enabled and that **Project Settings > Plugins > Mumble > Default Sound**, if set, points at a MetaSound that exists.
- **Silent, no warning.** Check that `BeginLine` or `Speak` is called, and that the component's attenuation lets you hear it from where you are. A component with no voice still speaks, in the built-in default, or in **Project Settings > Plugins > Mumble > Default Voice** when set.
- **Syllables go missing after a hitch or at high text speeds.** Expected: when several syllables come due in one frame, only the newest is spoken.
- **A custom MetaSound plays nothing.** The **Mumble Voice** node's **Stream** pin must come from the `Mumble Stream` graph input, not a literal.

## Options

Duplicate a voice in `/Mumble/Voices` and edit it, or create a **Mumble Voice** data asset.

| Group | What it changes |
|---|---|
| **Throat** | **Pitch Hz** sets the speaking pitch. **Tract Size** sets the size of the person: about 0.85 for a woman, 0.7 for a child. **Open Quotient**, **Skew** and **Brightness** shape the buzz from pressed to breathy. **Breathiness** adds air, **Subharmonics** adds gravel, **Creak** adds vocal fry at the end of a statement. |
| **Steadiness** | **Jitter** and **Shimmer** make each pulse slightly uneven. **Wobble Rate** and **Wobble Depth** add the slow tremor of an old voice (around 5 Hz, 0.5 semitones). |
| **Mouth** | **Mouth Openness** below 1 mutters. **Articulation** sets how clear consonants are: low is a soft mumble, high is crisp gibberish. **Consonant Cutoff** softens hiss. **Nasality** adds nasal tone. |
| **Prosody** | **Pitch Range** sets how animated stressed words are. **Declination**, **Question Rise**, **Exclamation Lift** and **Shout Lift** set the tune that punctuation and capitals give a line; **Expressiveness** scales all of them. **Randomness** varies each syllable, the same way every time for the same line. **Syllable Length** above 1 runs syllables together. |
| **Timing** | Only used by `Speak`: characters per second and the pause after each kind of punctuation. |
| **Seed** | Two voices with the same settings and different seeds say a line differently. |
| **Sound** | A MetaSound to voice this with instead of the default, for reverb, a radio filter or any other effect. It needs an Int32 input named `Mumble Stream` wired to a **Mumble Voice** node. |

### Edit the MetaSound

Every voice plays through a MetaSound Mumble builds at runtime (an Int32 `Mumble Stream` input wired through a **Mumble Voice** node to the output), unless the voice names its own **Sound**. To get an asset you can edit:

1. Open **Project Settings > Plugins > Mumble**.
2. Optional: set **Generated Sound Folder** (default `/Game/Audio/Mumble`).
3. Press **Generate Default Sound**. It writes `MS_MumbleVoice` to that folder, assigns it as **Default Sound**, and selects it in the Content Browser.
4. Open it and add effects after the **Mumble Voice** node. Every voice without its own **Sound** now plays through it.

Each press makes a new asset with a unique name rather than overwriting the last. To go back to the runtime MetaSound, clear **Default Sound**.

The shipped voices are generated by `Tools/create_voices.py`; edit the values there and re-run it rather than editing the assets:

```
UnrealEditor-Cmd.exe <Project>.uproject -run=pythonscript -script="<path>/Mumble/Tools/create_voices.py" -unattended -nopause -nosplash
```

English is read with spelling rules and word stress, kana as the syllables they are, and any other script as a syllable per character chosen by the character.
