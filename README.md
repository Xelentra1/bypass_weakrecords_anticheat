# Executive Summary
This program disables the anticheat application created by WeakRecords(www.GuidedHacking.com user).

## How the Anti Cheat works
The anticheat works by creating four threads that each do various things to circumvent a DLL from being injected into it's memory space. After creating those threads, the main thread goes into an infite loop ensuring none of it's child threads have been tampered with.

## Research Explanation
The binary by all accounts and measures appears to be benign. It is still encouraged that you work in a safe lab environment.

#### Basic Dynamic Analysis
Anytime you're working with an unknown binary, it's always good to start with behavioral analysis. What does the program do? What types of requests is it making? Where are these requests going? What files is it accessing? Registry Keys? Handles? Mutexes? There's nothing special about this process. You just run the binary and use wireshark to capture network traffic, Process Hacker to view the processes resources, and procmon(noriben.py) to analyze changes to the file system.

Running the program and trying to inject a DLL will result in several bad boy message boxes popping up resulting with a crash as soon as you click okay.

#### Basic Static Analysis
Next, you should use basic static analysis techniques to learn more about the application. I like to use PEStudio, Detect it Easy, and BinText here. Looking at it's import table, you'll be happy to see that the program isn't packed which is stated as such in the challenge description but it's good to verify. You can start to get a picture for how the application is implementing it's functionality and you can pinpoint interesting function calls to analyze further.

While there isn't necessarily any wrong answers here, two interesting functions that I'd like to explore further at this point are CreateThread and MessageBox.

#### Deeper Static Analysis
At this point, we are going to use a disassembler to glean more information. It is preferred that you use a disassembler with a decompiler. I opted to use IDA Pro but you'll be able to accomplish the same thing using Ghidra. Naviagte to where your tool lists the functions the binary imports and check what functions call MessageBox. You'll notice it's called in several places and that the strings passed to MessageBox are encrypted using an XOR algorithm. If you do the same thing with CreateThread, you'll realize that the MessageBox calls are mostly called by these threads.  

Let's take a quick break and put together what we have. Everytime you tried to inject a DLL, the application would create messageboxes letting you know it detected the injection. Every single thread that's created contains a call to MessageBox. Interesting no? What would happen if we simply never created those threads?

#### Deeper Dynamic Analysis
Let's pop this binary in x64dbg and place a breakpoint on every call to messagebox and NOP out the instruction calls to CreateThread and see what happens when we run the application. AH! If you've managed to follow along this far, you'd had noticed you still hit a breakpoint and get a MessageBox that states, "Abnormal Thread Behavior!" If you scroll up a little bit and click on the jumps to see which check is responsible for getting you to the MessageBox you'll find a call to SuspendThread followed by a possible jump if EAX is not equal to 0. Looking up the return value for SuspendThread you'll find this, "if the function succeeds, the return value is the thread's previous suspend count; otherwise, it is (DWORD) -1. To get extended error information, use the GetLastError function." This makes sense, the thread is immediately resumed after this call so the call to SuspendThread should never return anything but 0. That's why we've run into problems... since we patched the calls to CreateThread, SuspendThread is returning -1. You're only limited by your imagination in terms of possible solutions but I opted to simply patch the JE instruction to always Jump.

### Conclusion
You should now be able to inject a DLL without seeing any bad boy messageboxes popping up. Patching the binary itself seemed like a hack so I wrote a program that will programatically accomplish the same thing. I believe there might still be a check that I'm missing because once my DLL terminates, so does the AntiCheat application. This functionality doesn't occur if the binary is attached to a debugger though. ¯\\_(ツ)_/¯
