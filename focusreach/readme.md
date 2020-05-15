# focusreach

my myopia keeps worsening.
hence i decided to track it.
for measurements i use a standard self-retracting metal tape measure.
i taped a small piece of paper between the tape and the button.
i wrote a few letters onto the paper.
it's like a mini, 1 row snellen chart.
then i put the tape hook to my forehead
and retract the case just before the slightest blur starts.
i take note of the distance on the tape in millimeters.
this is how the measurement looks like from my eyes:

![measuring the focus reach](measure.jpg)

then i put that into the data file.
it's a semicolon separated list separated list of tokens.
the columns are the following:

```
datetime; central; left; right; tags; comment
```

central, left, right are distances in millimeters.
that's how far i see without glasses.
tags are comma separated words.
they describe either the environment of the measurement,
or the activity i did before the measurement.
environmental tags:

- bright: indoors but with lots of natural light.
- sunglight: directly under the sun.
- lit: in an artificially lit room.
- tired: felt tired as i was doing the measurement.

activity tags:

- sleep: right after some sleep or nap.
- nearwork: right after some near work without glasses (reading or writing).
- glassfree: walked around without glasses, almost blindly.
- reduced: used reduced glasses before.
- distance: i wore my distance glasses.
- exercise: i did some eye exercises.

not sure if i'll ever use those tags for anything,
but there's no harm in collecting them.

here is my progress over time:

![graph of the data](plot.png)

i generate the above graph using the plot utility from this repo.
