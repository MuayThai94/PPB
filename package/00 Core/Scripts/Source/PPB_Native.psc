Scriptname PPB_Native Native Hidden
{Minimal Papyrus bridge for Precision Physic Bodies. Backed by PPB.dll -> src/PapyrusNatives.cpp.}

; STATUE MODE (A-pose spell v3): while on, the DLL overwrites every bone of this actor with the
; skeleton's reference (bind) pose each frame — the deliberate "animations-failed" statue used as
; the calibration reference (straight elbows, flat wrists, planted feet).
Function SetStatuePose(Actor npc, Bool on) Global Native

; HEEL FIX (console-driven): while ON, every PLANCK-driven NPC wearing heels gets her ragdoll driven
; up to match her heel-raised visible body (touch zones, weld anchors, hit boxes all align). From the
; in-game console:   cgf "PPB_Native.ToggleHeelFix"      (or SetHeelFix 1|0)
Bool Function SetHeelFix(Bool on) Global Native
Bool Function ToggleHeelFix() Global Native

; GESTURE PAUSE (2026-08-27, with the hand-gesture device layer's move into PPB): stand the layer
; down so a scripted scene placing an actor's hands is not read as a grab. While paused, no equip,
; undress or plug gesture is detected and any in-flight gesture state is cleared.
;   PPB_Native.SetGesturePaused(True)   before a scene poses hands
;   PPB_Native.SetGesturePaused(False)  after
; The add-on's legacy VRTE_DDZaZ_Native.SetScenePaused still works and forwards here, but new
; callers should use this directly.
Function SetGesturePaused(Bool abPaused) Global Native

; UPRIGHT (2026-09-19): level an actor whose BODY TILT is stuck — the "crooked NPC standing a few
; units off the floor, animating and talking but never straightening" state. That tilt lives on her
; character controller (pitch/roll), NOT on her reference rotation, so SetAngle/Disable/Enable and a
; cell reload can never clear it; the engine's own gradual fix only runs while she MOVES, so a bound,
; posed, held or dead actor keeps it forever.
; PPB levels her automatically once the tilt has failed to shrink for `uprightGraceS`. Call this when
; your script KNOWS the moment instead — a knockout's wake, a capture mod's release, a pose ending:
;   Float tilt = PPB_Native.UprightActor(akActor)
; Returns the tilt it found in DEGREES (0.0 = she was already upright), or -1.0 if it refused:
; she has no character controller, her race is allowed to pitch/roll (quadrupeds, flyers), or she is
; in a true ragdoll (nothing to level while physics owns her — call it after she is up).
Float Function UprightActor(Actor akActor) Global Native
