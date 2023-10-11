# Actually Baby Rev

After reverse engineering the code, it is evident that there are 3 stages to the challenge. Each challenge accepts a string of digits ([0-9]+) and performs some sort of check on it that you need to pass.

NOTE: feel free to look at the source code for the challenge, but it has some extra bits of reversing to do haha which compiling had simplified since they were static. 

## Stage 1: the `gauntlet_of_happiness`
Here you are only allowed to use 5s and 9s in your strings of digits so the regex is [59]+. There is a happiness value that must reach exactly 127 (starting from 0). Reversing each case, one can deduce that every instance of 5 adds 5 to happiness gauge. Similarly, every instance of 9 adds 4 to the happiness gauge (in quite a long winded way tbf, but hey I'd argue this demonstrates the baby's emotional turmoil upon seeing the cat toy and promptly relishing in the act of violence it commited in throwing it :P). Now, you can do some fancy math in order to find an integer combination of 5's and 4's that add up to 127. My favorite way personally is to graph `5x + 4y = 127` on desmos and check for integer points. Once we do we find out the answer would be:

`5555555555555559999999999999`   


## Stage 2: the `gauntlet_of_hunger`
Here you are only allowed to use 1, 2, 4 so [124]+. However, we can quickly see `1` feeds the baby some sugar which is a big nono and the program exits because you were being a bad caretaker. There is a hunger meter that starts at 0 (well it really should be a lack of hunger meter) and needs to satisfy the following condition: ((hunger % 3 == 1) && (hunger % 4 == 3) && (hunger % 5 == 1)). This is a quick little lesson in Chinese Remainder Theorem, but essentially the answer to this would be any number such that (hungerr % 60 == 31). Case 2: feeds it some baby food, which adds 13 to the hunger meter. Case 4: Prepares some milk to feed the baby, which adds 6 to the hunger meter (milk feels like its all a baby needs, but honestly it's not great as their only nutritional source after a certain age smh). Using our glorious technique from earlier, it's trivial to deduce that the right combiination is `2444`.

## Stage 3: the `gauntlet_of_sleep`
The baby is still crying and we must just put it to sleep in order to stop the tears. We are now trying to fill up a sleep meter that starts at 1 and needs to be 2310. We are allowed to use the following regex [03678]+. This time, it goes a little faster since we are multiplying values. Now, our baby has a little nighttime ritual that it must follow unless you want a fussy baby in the morning.

Case 8: Swaddles the baby and sets the sleep meter to a 3.
Case 0: Reads an epic tale about a boy who wants to be king of the pirates, which checks whether the sleep is at 3, and then multiplies it with 7.
Case 6: Now you must rock the baby in your arms. It will make sure that its sleep is at a 21 before multiplying it with a 5.
Case 7: Then you sing it a lullaby that your mom sang you. It will make its sleep is at a 105 before multiplying with 11 (lullabies do make babies sleepy).
Case 3: Finally, once you've made the baby sufficiently sleepy so its sleep == 1155, tell it that bigfoot will come for it if they don't go to sleep this instant. The sleep reaches 2310 as required. 

`80673` is our magic string.

And tada the flag is unlocked. 