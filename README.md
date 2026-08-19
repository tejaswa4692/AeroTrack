# Aero Track

Why should directional antennas be handicapped by the fact theyre directional, i made a rig (can be used with any directional antennas) that can be used to track a target anywhere on the earth
It will do all the math to point the antennas towards a GPS coordinate, It does have its own GPS but if you dont want to use it its totally upto you its just there for quality of life and ease of use at the field.
This project was developed to aid my Long range endurance UAV project that i also submitted for stardance and got funding from, This will help me make a groundstation that can have insane range without sacrificing directionality

___

I will be implementing full support with the Mavlink protocol so you can either directly plug ur mavlink telemetry reciever in or route the data through a laptop

## System Architecture

It uses two stepper motors with encoders for pitch and yaw 

```
base.rotation.y = atan2(local_target.x, local_target.z)
antenna.rotation.x = -atan2(local_target.y, Vector2(local_target.x, local_target.z).length()) + deg_to_rad(90)
```
These two formulas will be used to point the yagi towards the location i want it to point to, which will ofcourse be GPS coords in this case

I have tested this in my game which i ALSO submitted for stardance and got a superstar on 

### Electronics

[bom.csv](./bom.csv)

### Antenna

Yagi is a highly directional antenna which has insane range for anything thats directly infront of it, its used a LOT in TV communication and for groundstations where they know the position of the gs and they directly point it to that point
Hugely used in aerospace 

## Wiring

![Yagi Tracker](./assets/Group25.png)
