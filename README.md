#  Library Management System (C++ | SQLite)

A console-based Library Management System developed using C++ and SQLite,
demonstrating Object-Oriented Programming, database integration, and exception handling.

##  Features
- Role-based access (Student / Librarian)
- Add and manage books with inventory tracking
- Borrow and return books
- Persistent storage using SQLite
- Custom exception handling
- STL-based data structures

##  Technologies Used
- C++
- SQLite3
- STL (vector, algorithm)
- Object-Oriented Programming

##  Database Schema
- USERS (ID, NAME, ROLE)
- BOOKS (ID, TITLE, AUTHOR, COPIES)
- TRANSACTIONS (USER_ID, BOOK_ID)

##  How to Run
```bash
g++ main.cpp -lsqlite3 -o library
./library
