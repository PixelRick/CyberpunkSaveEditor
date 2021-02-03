# A quick summary about the vulnerability I found in the game

update: game patch 1.12 is now available and fixes both vulnerabilities, thanks cdpr !

The vulnerability impacts DATA files. A buffer overflow can be triggered in the game when it loads those files.

Reading the reddit posts, I think that the average commenter has not enough creativity to imagine all the ways it can be exploited.
Someone said you have to download a mod to be at risk. But doesn't see that steam/gog etc use clouds to save your save games.
I could email-spam people with steam accounts logins with CP77 and an infected save in the cloud.
Would you honestly have been cautious using this account ??

The reason why a buffer overflow can be triggered is that the game uses a buffer of 512 bytes to serialize a maximum of 512 wide-characters for identifier strings, and that is 1024 bytes (a wide-character is 2 bytes).
This buffer overflow can be exploited with the help of a second vulnerability that is a third-party library that the game uses: xinput1_3.dll. This dynamic library is not relocatable and thus is a direct bypass for a security feature called ASLR (Address Space Layout Randomization).
Also, this library is enough to build a ROP-chain to bypass DEP (Data Execution Prevention) in order to execute code that has been inlined in the overflowed buffer.
(This ROP-chain won't be disclosed any time soon as it represents a risk not only for CP77 but for every piece of software using it..)

I chose to work on the most scary scenario that is code hidden in harmless-looking save files. When the game does read this file it uses a specific reader object that I use in the shellcode to read more after the 1024 bytes string and thus load an even bigger shellcode.. this second shellcode does different things: it hides the exploit from the file reader so that it can load the original save file afterwards; it reads a payload dll and manually maps it; it repairs the stack to be able to call load_save again; it repairs other things i won't disclose here; it does a copy of the exploit in memory; it hooks the file writing method to be able to inject the exploit in future save files when the game does save or auto-save..
At this point it is what we could call a virus-dropper worm that would use save files sharing to spread.
Don't worry though, the only version I shared with trustworthy people is one that crashes the game and has no worm capabilities at all.
But it is possible someone else found it earlier and kept the information secret, and that's why cdpr relayed the warning to the community.

This is a shared responsiblity between CDPR for the buffer overflow and Microsoft for not providing a safe backward compatible version of xinput to companies in need.
CDPR did fix the buffer overflow internally, and this fix is expected to arrive with one of the next two patches.

Thanks to yamashi who is currently protecting people from this exploit by patching the first vulnerability dynamically with his mod https://github.com/yamashi/CyberEngineTweaks/ that is used by many.

Please mind that it is only about data files.
Once the vulnerability is patched, only data files will be safe to use again (texture, model mods, saves, etc..), whereas executable mods will remain potentially dangerous and will always be (so at least check their authors and comments about them first).

There wasn't any bug bounty program so I received peanuts for the discovery.

If you wish to thank me for it, I wouldn't be against being offered a cyberpunk t-shirt ;)

mini comments:
- Moonded offered me a trauma team helmet t-shirt :P
- @Proto2149: yamasushi on the forum is yamashi.

# CyberpunkSaveEditor
A tool to edit Cyberpunk 2077 sav.dat files (join the CP modding discord: https://discord.gg/cp77modding)

![](./basilisks.png)

This is a holidays project and will probably not reach the user-friendly GUI state that a save editor is expected to have.

If you are looking for an intuitive editor, please take a look at the other save editor project made by a group of C# developers on the CP modding discord:
- https://github.com/Deweh/CyberCAT-SimpleGUI (forked from https://github.com/WolvenKit/CyberCAT).

# Install
To download CPSE as an executable, please click on Releases in the right pane as shown in this screenshot:

![](./tuto_github.png)

# What works
1) load, save node tree
2) mini hexeditor for nodes data (can change node data size)
3) search tools (string, crc32(namehash) of string, u32, float double, from hexeditor clipboard)
4) [experimental] copy/paste skin blob between saves
    (this can fail for unknown reasons yet between fresh save and high-level save)
5) inventory editor (most fields are still obscure and some item names are not resolved)
6) can unflag all quest items to make them normal items
7) can add stat modifiers to items!
8) can edit the scriptables data in system nodes.

# Preview
![](./preview.png)

# Roadmap
1) Code cleaning
2) Work on github issues
