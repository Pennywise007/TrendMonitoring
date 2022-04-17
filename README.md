What is this program for?
There is a working object on which sensors are installed that take various indicators (flexometers / thickness gauges, etc.). The data from the sensors is transmitted via GSM to a PC and recorded by a separate program in the form of binary files according to a specific scheme. A simplified representation of the operation of this scheme: each sensor has a descriptor file with information about the sensor (its name, sampling rate, unit of measurement, etc.).
Files are laid out according to a certain hierarchy in the necessary folders and stored there. In the future, using these files, separate programs can build a signal trend from each sensor and look at changes in sensor readings depending on time.
<p align="center">
<a href="https://ibb.co/hZPZzgx"><img src="https://i.ibb.co/xfpfT2K/image.png" alt="SignalTrends" border="0"></a>
<p align="center">1. An example of signal trends.</p>
</p>

This program monitors the readings of sensors in real time, decrypting data files, provides data in a readable form and notifies operators if the values ​​exceed the allowable level. This allows you not to monitor the readings all the time, but to monitor remotely. The system integrates work with telegram bots, each object runs its own copy of the program with its own bot, which allows you to manage / monitor many systems simultaneously from one (or several devices). In the event of a problem, operators are alerted to the problem, allowing them to take further action. Supports daily reporting on the status of the system with data analysis. This system is currently installed at several sites in Moscow.
<p align="center">
<a href="https://ibb.co/gtTR2hF"><img src="https://i.ibb.co/wpWzkjr/image.png" alt="Monitoring" border="0"></a>
<p align="center">2. A running program is a program.</p>
</p>


<b>Program architecture</b>

I tried to make the program modular, there are two projects - Core, in which all the "brains" of the program and the UI part are located.

<p align="center">
<a href="https://ibb.co/wWRPHwR"><img src="https://i.ibb.co/wWRPHwR/image.png" alt="image" border="0"></a>
<p align="center">3. Program architecture.</p>
</p>

<b>Core</b>

The static library where all the magic happens. Several services are exposed:
1. ITrendMonitoring - the main object of the library, through which the user interacts. Here the current system settings are stored, the telegram bot is configured, data request tasks are launched by channels, reports are generated, and errors are notified.
2. IMonitoringTasksService - a service with monitoring tasks, there is a queue of tasks for receiving data from files.
3. IDirsService - an auxiliary general service for working with directories for downloading trend data
4. ITelegramBot - a service for sending and receiving data from a telegram bot with various parsers of messages and user commands.

As for the telegram bot, the program is divided into administrators (operators) of the system and ordinary users (customers). This division is intended to minimize customer "twitching" due to problems, because sometimes sensors can give false alarms (a wire may go somewhere, technical work, etc.). Users have a separation of available commands, and some commands support the creation of buttons for quick response or easier interaction with the bot. The resulting callbacks are parsed using boost::spirit and regex. Initially, each request was parsed according to its structures, but there were a lot of requests and everything was unified to simplify support. The main work with the bot, data exchange and work with the telegram API is carried out through my separate DLL, you can read more about it here: https://github.com/Pennywise007/TelegramDLLPublic
<p align="center">
<a href="https://ibb.co/wSmV1ZJ"><img src="https://i.ibb.co/N9B5zg1/image.png" alt="BotExample" border="0"></a>
<p align="center">4. The work of the bot and the reaction to commands.</p>
</p>


<b>UI</b>

Here is the entire UI part of the application, in particular
1. Several custom tables (wrappers over the Windows CListCtrl) implemented as decorators to separate functionality. Implemented cell editing with the ability to insert various controls for this, filters / sorting / groups, etc.
2. Custom selection list with regex search, which allows you to quickly and conveniently navigate the list of sensors (on some objects there are about 50 different sensors). This control is inserted into the table when editing.
3. Custom lists and input fields with SpinEdit.
4. Own layout for scaling forms and expanding the functionality of MFC.
5. An auxiliary class for working with the tray icon and displaying Windows notifications (Shell_NotifyIcon) in case of new events.
6. Dialogs and tabs


<b>Tests</b>

The project for testing through googletest, performs unit testing of the program mechanisms.
 - performs data analysis checks (comparing with pre-recorded standards)
 - checks the results of serialization / deserialization (compares with standards)
 - emulates and checks the work of the bot with the user
 - check the user's work with the list of channels
<p align="center">
<a href="https://ibb.co/Pzw4rPR"><img src="https://i.ibb.co/Pzw4rPR/Untitled.png" alt="TestResults" border="0"></a>
<p align="center">5. Test results.</p>
</p>


<b>Additional information</b>

The application is written in C++17 and adapted for C++20, technologies used:
- boost (asio/program_options/spirit/fusion)
- std (regex/algorithm/filesystem/thread/future/mutext/chrono/functional/memory/optional and much more) - tried to do std as much as possible than on boost, plans to expand the functionality in C++20
- other (AtlCOM, MFC, afx, OpenSSL, STL, Google test + mock)
- pugixml
- library for telegram bots with its dependencies https://github.com/Pennywise007/TelegramDLL
- own library with various C++ extensions (Dependency injection, Reflection for Serialization, Thread pool, Events, Dispatcher) https://github.com/Pennywise007/Ext
- own library with various controls for MFC https://github.com/Pennywise007/Controls