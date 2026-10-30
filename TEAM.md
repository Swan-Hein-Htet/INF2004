# Team integration map

The supplied template's five subsystem folders are retained. Detailed interfaces,
pins, priorities, ownership changes, assumptions and tests are documented in
README.md and docs/WIRING.md. No tutorial LED or producer/consumer task remains.

| Subsystem | Design owner | Code |
| --- | --- | --- |
| 1 | Swan Hein Htet | mission/protocol/network integration |
| 2 | Charmaine Wong Shi Qi | wheel PID, encoders and board adapter |
| 3 | Yun Cong | line and Code 39 reader |
| 4 | Toh You Yang | accelerometer terrain and safety policy |
| 5 | Danish Hazwan | ultrasonic scan profiling |

T6 is the sole mission/motion-goal owner. T5 supplies profiles rather than queuing
competing motor goals. Only T4 writes running motor PWM; T1 and the E-stop ISR
may force it to zero. All GPIO interrupt sources share one kernel dispatcher.
