# Message Persistence (Chat History)

## Overview
This feature adds file-based message persistence to the chat system. Messages are stored per room so that users can see previous chat history even after reconnecting or restarting the server.

---

## How It Works

- Each chat room has its own file:

<room_name>.txt


- When a message is sent:
- It is appended to the room’s file
- It is also broadcast to active users

- When a user joins a room:
- The server reads the room file
- Sends all previous messages to that user

---

## Example

If users chat in a room named `general`, messages are stored in:

general.txt


File content:

Alice: Hello
Bob: Hi
Charlie: Welcome


When a new user joins `general`, they will see:

Alice: Hello
Bob: Hi
Charlie: Welcome


---

## Benefits

- Preserves chat history across sessions  
- Allows new users to see past messages  
- Adds basic durability to the system  
- Simulates real-world backend behavior  

---

## Limitations

- Uses plain text files (no database)  
- No message timestamps  
- No file size management  
- Not optimized for large-scale usage  

---

## Summary

This feature ensures that chat data is not lost when the server restarts and provides a simple way