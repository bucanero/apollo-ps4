# Usage Instructions
# NOTICE you must have a fake activated account or already have a real PSN account on the console to use saves from other users and properly export savedata. 
* See `Activating offline profiles` on how to do it by using your real PSN ID or allow Apollo to create one.

# Title ID, User ID and Account ID 
## What is a Title ID 
* The product code is distinct to a region, and the common codes you’ll see are CUSA, PCAS, and PLAS.  
R1 USA-CUSA        
R2 Europe-CUSA     
R3 Asia-PLAS, PCAS        
   
The product code is followed by a 5 digit unique number identifier.      
The Title ID is the Product code along with the unique numerical identifier of the game.     
* For example Minecraft USA is CUSA00744 while Minecraft EU is CUSA00265    
* Another example Resident Evil 2, USA is CUSA09193 while Asia is PLAS10335   
* The majority of games you come across will use CUSA.    

## What is a User-ID 
The user ID is the internal description for the local user account. (Example - 11cd8de)

## What is an Account-ID
The Account ID a PSN ID assigned to the local user. (Example - abcdef01234556789) but must be 16 hex characters.
* To see yours and active your offline console's user account with it or to generate a fake one for fake activation read below.

# Activating offline profiles
* You can offline activate your local account on a jailbroken console to either match your legitimate PSN Account ID which will allow you to directly transfer save files via USB with no need for Save Wizard. 
* Or activate a local account for use with chiaki and and proper savedata management.
## To fake activate the account and make it match your PSN Account ID and use your PSN saves and chiaki 
1. On a console that has your legitimate PSN account copy a save file to a USB Drive.
2. Plug in the USB Drive into a PC
* On the USB you will find the following folder path.
* `PS4/SAVEDATA/abcdef01234556789/CUSAxxxxx`
* abcdef01234556789 (is an example here) but for you the one you see there is your PSN account ID take note of it for later 
3. Open Apollo Save Tool, navigate to User Tools.
4. Select Activate PS4 Accounts.
5. Select your profile.
6. Select the account you want to activate and press X.
7. A string of letters and numbers will show up you can replace them with the PSN account ID you got earlier.
8. Then press R2 then X and then keep pressing O till you are asked if you want to exit to the XMB accept then restart the console or sign out and back in.
9. Any saves you have can now be transferred via USB to a legit console as long as the title id matches and the game version is the same or higher.

## To fake activate for use with chiaki and general save management 
* Ignore if you have done the above option and already have an activated account go to Getting the ID needed for Chiaki if needed.
1. Open Apollo Save Tool, navigate to User Tools.
2. Select Activate PS4 Accounts.
3. Select your profile.
4. Select the account you want to activate and press X.
5. A string of letters and numbers will show up copy them CORRECTLY then press R2 then X and then keep pressing O till you are asked if you want to exit to the XMB accept then restart the console
*(the string can be found in About in Apollo Save Tool incase you lose it)

## Getting the ID needed for Chiaki
* This works for both the PSN account ID activated profile and for a normal Apollo fake activated account.   
1. You will take your Account-ID and convert it on the website below.    
* Already loaded here > https://trinket.io/embed/python3/ba2ff26973    
3. Once you are ready to run the code paste the string of the ACCOUNT-ID (example > abcdef01234556789) in the user_id = "here" between the 2 "" as seen in the code then run it. (ignore that it is named user-id in the script it is the account-id)
4. You will then receive a new string of letters and numbers (example > CN8aubEclS6=) you can then use that with chiaki to use remote play or continue to utilise the offline activated account for savedata management.


# Getting saves and or managing them 
There are many ways to download or manage savedata here are some of the easiest ways.   
## Online database  
The online database is a ever growing collection of save files for PS2, PS3 & PS4 games. Users can submit their own save files too.
1. Simply navigate to online database section, select your game, and download to a USB drive or to your internal HDD.
2. Navigate to either USB saves or HDD saves, depending on where you chose to save them.
* If you selected HDD saves, check to see if they appear in the HDD saves section.
* If you selected USB saves, navigate to USB Saves and select `Copy save game` to HDD.

## Copying saves between multiple profiles
This is useful if you have multiple accounts like User1, User2 etc.
1. Simply navigate to HDD saves  `Copy save game` and select to USB.
2. Log in to your secondary profile.
3. Navigate to USB  and select `Copy save game` and select the HDD.

## Managing your Decrypted save data 
1. You can export and import decrypted savedata by using the option `Copy save game` in Apollo. Or the Export/Import Decrypted save files options but they send the data to the internal drive.
2. When using the `Copy save game` option Apollo copies decrypted savadata to the following path
*  `/PS4/APOLLO/ <USER-ID><title-id><Save-name`       
with files and a sce_sys folder inside.   
3. When importing it not needed for the folder path to end with  
* `<USER-ID><title-id><Save-name` the `title-id` is enough.      
But a proper structure with savedata and a sce_sys folder is needed.
4. When you decrypt a save file you are then one step closer to editing it if tools or documented methods exist for the specific game.
To copy it back to the HDD later select `Copy save game` and select the HDD.

## Managing Encrypted save data tied to your account
You can import and export savadata by using the Application Saved Data Management option in Settings on the PS4.
* This exports encrypted saves signed to the account id of the user exporting them and can usually only be used on the same user even on a different console unless Apollo or Save Wizard are used.
* To use your savedata on a jailbroken and retail console see Activating offline profiles. Otherwise export and import as needed. Using the option `Copy save game` or `Export save game to Zip` in Apollo and backing it up to a USB is suggested as it gives you decrypted savedata.

## Managing Encrypted savedata acquired online
Encrypted saves are saves made from retail/unjailbroken PS4s made via Account Data Management feature or come from Save Wizard. 
* Note that Save Wizard is not needed. 
* Note that the saves must be from 11.00 or lower.
1. On a USB drive formatted as exFAT, put the saves in the following structure: `/PS4/SAVEDATA/ <ACCOUNT-ID> / <title-id` and inside the ACCOUNT-ID should be a folder with the title id of the game CUSAxxxxx and inside it the save files.
* The ACCOUNT-ID can be  `0000000000000000` or `abcdef01234556789` or any 16 hex characters. 
* The title-id is that of the game for example `CUSA09193`. 
2. Back to the PS4, open the game the save was made for, and make a brand new save. 
3. Open Apollo Save Tool.
4. Navigate to USB Saves. Select the save.
5. Select copy to HDD. Select yes to resign it.
6. Check to see if the save is present in HDD Saves.
* Note sometimes save files may display a ??? firmware warning in this case try the following.
7. Delete all savedata of the game on the HDD and create new savedata.
8. Export it via the Account Data Management feature from settings. Or for decrypted data `Copy save game` to USB.
9. Compare the online savedata names and make it match the one you just got from the console then try again.

## Managing Decrypted savedata acquired online
Decrypted saves are usually your own but sometimes are shared online.
* Note that Save Wizard is not needed. 
* Note that the saves must be from 11.00 or lower.
* If the save file you have has a sce_sys folder with a keystone and param.sfo
1. Put it in the following path on a USB drive formatted as exFAT `/PS4/APOLLO/<title-id` and in Apollo go to USB saves choose the `Copy save game` option and copy it to the HDD.
* If the save file is a file on it's own.
2. Plug in a USB drive formatted as exFAT into the PS4.
3. You will need to create a save in the game and then from Apollo in HDD saves use the `Copy save game` to copy the savedata to the USB.
4. Then in this path on the USB `/PS4/APOLLO/<USER-ID><title-id><Save-name` find and replace the file inside with the file you alread have then back in Apollo in USB saves use the `Copy save game` option and copy it to the HDD.

## Managing save files from different CUSA IDs
This is a fix if you want to make use of files that are for the same game but different CUSA IDs or versions (such as standard vs deluxe/GOTY editions).
1. On the game you want the save to be applied to, make a new save.
2. Open Apollo Save Tool, select HDD saves. Select your save and select Export decrypted save files. Export all the files one by one.
3. Open PS4Xplorer, navigate to `/data/apollo/yourprofileid/` and simply rename the folder of the CUSA ID to the version of the game you need to have the game to.
4. Back to Apollo Save Tool, select HDD saves. Select your new game title ID, select Import decrypted save files. Import them one by one. Select Apply Changes & Resign.
5. Open your game. You should see the same name as before, but a different size or at a different point in the game.

## Fixing Keystone issues with savedata 
*Assuming the title id of the game or game version are not the issue sometimes you may do everything right but savedata always shows as corrupted. 
*This usually means the game you have or the game the save came from does not have a matching keystone this is usually an issue with dumped games as they are not rebuilt with the original keystone.
1. To fix this export the keystone from an already working save file and import it into the save file that you want to fix then copy the save file to the HDD and try it out.

# I rebuilt my database and now all my homebrew and games are gone how do i get them back?
1. Download the latest version Apollo Save Tool and put it in the root of a USB drive ( Has to be formatted in Exfat) then plug it into your PS4. 
2. Jailbreak your console as you usually do.
3. In GoldHEN Debug Settings before entering the Package Installer under it are some options make sure Enable Background Installation is NOT enabled.
4. Install Apollo and select yes to reinstall it if it asks you to ( This happens only if it was previously installed)
5. Open Apollo Save Tool and go to User Tools.
6. Then select App.db Database Management.
7. There you can select Rebuild App.db Database (Restore missing XMB items) and you can also select Rebuild DLC Database (addcont.db)
8. Then close Apollo and log out of and log in the user account for everything to properly show up.
* If you cannot see the games and apps on the home screen check in the PS4 Library App.
* Optionally you can then create a backup of your current database to use in the future with the restore option.