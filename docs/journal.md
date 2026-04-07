# Developer Journal

A chronological log of work done in the lab, daily goals, and simple tasks.

---

## 20 March 2026
**Time:** 14:16
**Location:** Rankine Lab 509

**Goals:**
- Identify the correct water pump and replace the solenoid valve setup.
- Figure out how to safely connect the pump to the Raspberry Pi.
- Resolve 5V power distribution across the various sensors and the pump.

**Notes:**
- We decided to switch back from the 12V Solenoid valve to a submersible pump because the solenoid was bulky and not performing as expected with the relay setup.
- Explored powering the pump. Tried direct pi connection (Bad Idea). Explored 3x 1.5v AA batteries (didn't go this route). Ultimately wired a T1P121 transistor with a flyback diode and a 1000 ohm resistor. 

---

## [Date TBD] 
**Time:** 11:23
**Location:** RTES lab

**Tasks:**
- [x] Connect the gesture sensor and have it working. (Read APDS-9960 specs at 11:26)
- [ ] Make a social media plan. We need to upload 5 times and get people to like.
- [ ] Research about the 3D printing.

**11:33 Subtask:**
- [ ] Find out how to connect the gesture sensor to the Raspberry Pi.

**Notes & Observations:**
- The wires we use are quite short and in some cases the connections are weak.
- Right now this is no big deal because we are testing our connections separately, but this will become important when we are bringing the product together.
