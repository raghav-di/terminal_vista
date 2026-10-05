🌧️ Terminal Rain

A simple C++ terminal application that creates a relaxing rainy atmosphere directly in your terminal. It renders animated falling rain and plays generated rain sounds simultaneously, giving the effect of sitting beside a rainy window while working in the command line.

Demo

               :                              ::                 :           :           :    
                                  :           :                              :           :    
                                  :                    :  :                  :           :    
                                  :                    :  :                                   
                                  :                    :  :                                 : 
                                                       :  :                                 : 
                                                       :                                    : 
                               :                                                            : 
                               :                                                            : 
        :                      :                                                              
        :                      :                                                         #    
       :                                                                                #     
       :                                                                                   #  
       :                                                                                  #   
       :                       :                                                        #  :  
            :                  :                                           :             # :  
            :                             :                                :             . :  
            :                      :      :                 :              :         ### . :  
     :      :                      :      :               : :              :        ###### :  
     :      :       :              :                      :::              :       #######    
     :              :              :                      :::                     #########   
     :              :                                     :::                     #########   
     :        @     :                                      :                      #########   
              |     :                                      :   :                  ####|####   
          :   |                           @    @               ::      :          ####|####   
==========================.============.======================================================
==============================================================================================
(Animated in the terminal with accompanying rain audio.)

Features
🌧️ Animated rain effect in the terminal
🔊 Procedural generated rain sound
⚡ Lightweight and native C++
🖥️ Runs entirely from the terminal
🎯 No external assets required for audio generation
Project Structure
.
├── audio.h      # Rain sound generation and playback
├── rain.cpp     # Main application and rain animation
└── rain         # Compiled executable

Build

Using g++:

g++ rain.cpp -o rain


Run:

./rain

How It Works

The application combines two components:

Visual Rain

Randomly generates and animates raindrops across the terminal window.
Continuously refreshes the screen to simulate falling rain.

Rain Audio

Generates rain-like white noise programmatically.
Plays the sound alongside the animation to create an immersive experience.
Requirements
C++ compiler with C++11 (or newer) support
Terminal capable of handling screen refreshes
Audio output device (for rain sound)
Credits

The rain sound generation code is based on software released under either:

The Unlicense (Public Domain)
MIT No Attribution License

Original Copyright:

Copyright 2026 David Reid
