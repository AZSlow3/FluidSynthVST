Here I describe all VST3 related problems I hit during development.


## MIDI
It is well known that VST3 has tried to kill traditional MIDI. That is partially
resurrected in recent versions, obviously no way to output arbitrary MIDI was
killing VST3 for many applications (MIDI FX).

- Notes, pithwheel as some other MIDI messages are available as Events during processing. The documentation does
  not mention they should be in any particular order, but I guess from common sense they
  should be time ordered.
- CC are Parameters. So ANY possible CC should be declared as a separate Parameter (16x128 parameters just for that).
  The schema is following: when CC is spoted, the plug-in is asked for possible mapping between CC (using bus, channel,
  and CC number) and particular parameter. Plug-in implement that in getMidiControllerAssignment.
  CC value is then delivered as (normalized) value for returned parameter
- PC are also Parameters. But making that work is worse then with CC. Host first ask about mapping using getUnitByBus,
  for bus/channel. In returned Unit, the host theoretically should find kIsProgramChange parameter and use it. In practice,
  I have not managed to make it work in REAPER without defining associated ProgramList. So I had to create
  16x128 "build-in presets". REAPER can not display all of them in any case, so that just has "polluted" the list.


Since Notes and CCs are transferred using different routes, they should be ordered first to reconstruct original MIDI
sequence. Parameter changes can have several "points" withing one buffer, so the sequence of received CCs, unlike notes, can not be
assumed as ordered

## Parameters in processor and controller
VST3 prescribe completely separate Processor from Controller, up to the level they can run on different computers.
The intention is good, the implementation is...

Parameters with all characteristics are definied within Controller. But SDK does not provide any help to get characteristics in the Processor.
If some Parameter is thought to be integer/string, the Processor still gets just normalized values and has to "reinvent the wheel" (Controller
has all conversion methods already, to feed host). Examples in SDK do not show any other way.

The situation is worse with String parameters. In my case, I try to build the list of possible build-in presets from the list of files.
The files are on Processor side, the parameter definition is on Controller side. So I have to compose the list and use IConnectionPoint to
transfer this list to the Controller. SDK has no reasonable serialization for lists, they have decided to "name" all "Attributes", but the
only types are "string" and "binary". No list. Buffer definition also does not support lists.

So, with environment dependent string based parameter:
- Processor compose the list of strings
- Processor sends this list to Controller
- Controller fill the ProgramList/Parameter, here there is no methods to "reset" them.

When and what is transferred between Processor and Controller is not defined. It can not be assumed getState will be before setComponentState,
at least REAPER all setState and setComponentState from the same saved data. That means that Processor and Controller should independently
able to process saved (potentially old) format preset.

In addition, processor has no change to change preset before controller get it. So if some parameter is environment dependent, as the list
of files in my case, processor is forced to send Message in its setState so Controller will not get incorrect value from setComponentState.
It seems like a bad idea to do this from process since there is no prescription to call it in sync.

## Lengthy operations on parameter changes.
Since switching build-in presets is indicated as a Parameter change, Processor get it in the Realtime thread.
How to say the processor needs some time to prepare? I have not found any regulations. Everywhere there is an aswise
"do not do anything blocking in processing". But I think a good plug-in framework should define some way for that case, no?

I am creating a separate thread and not calling the synth in between.
