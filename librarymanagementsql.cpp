#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <exception>
#include <sstream>
#include <limits> 
#include <sqlite3.h> 

using namespace std;

// ==========================================
// 1. Exceptions
// ==========================================
class LibraryException : public exception {
protected: string message;
public: LibraryException(string msg) : message(msg) {}
    const char* what() const noexcept override { return message.c_str(); }
};
class BookNotAvailableException : public LibraryException { public: BookNotAvailableException() : LibraryException("Error: Book not available.") {} };
class InvalidUserException : public LibraryException { public: InvalidUserException() : LibraryException("Error: Invalid User.") {} };
class InvalidBookException : public LibraryException { public: InvalidBookException() : LibraryException("Error: Invalid Book.") {} };
class PermissionDeniedException : public LibraryException { public: PermissionDeniedException() : LibraryException("Error: Permission Denied.") {} };

// ==========================================
// 2. Book Class
// ==========================================
class Book {
private:
    int bookId;
    string title;
    string author;
    int numberOfCopies;

public:
    Book(int id, string t, string a, int copies) 
        : bookId(id), title(t), author(a), numberOfCopies(copies) {}

    int getId() const { return bookId; }
    string getTitle() const { return title; }
    string getAuthor() const { return author; }
    int getCopies() const { return numberOfCopies; }

    bool isAvailable() const { return numberOfCopies > 0; }
    void decreaseCopy() { if (numberOfCopies > 0) numberOfCopies--; }
    void increaseCopy() { numberOfCopies++; }
    
    void display() const {
        cout << "ID: " << bookId << " | Title: " << title 
             << " | Author: " << author << " | Copies: " << numberOfCopies << endl;
    }
};

// ==========================================
// 3. User Class Hierarchy
// ==========================================

class User {
protected:
    int userId;
    string name;
    string role; 

public:
    User(int id, string n, string r) : userId(id), name(n), role(r) {}
    virtual ~User() {}

    int getId() const { return userId; }
    string getName() const { return name; }
    string getRole() const { return role; }

    virtual string serialize() const = 0; 

    virtual void display() const {
        cout << "[" << role << "] ID: " << userId << " | Name: " << name << endl;
    }
};

class Student : public User {
private:
    vector<int> borrowedBookIds; 

public:
    Student(int id, string n) : User(id, n, "Student") {}

    void borrowBook(int bookId) { borrowedBookIds.push_back(bookId); }
    bool returnBook(int bookId) { 
        auto it = find(borrowedBookIds.begin(), borrowedBookIds.end(), bookId);
        if (it != borrowedBookIds.end()) {
            borrowedBookIds.erase(it);
            return true;
        }
        return false;
    }

    string serialize() const override {
        string data = "S|" + to_string(userId) + "|" + name + "|";
        for (int id : borrowedBookIds) data += to_string(id) + ",";
        if (!borrowedBookIds.empty()) data.pop_back(); 
        return data;
    }

    void loadBorrowedBooks(string idsStr) {
        borrowedBookIds.clear();
        stringstream ss(idsStr);
        string segment;
        while (getline(ss, segment, ',')) {
            if (!segment.empty()) borrowedBookIds.push_back(stoi(segment));
        }
    }

    void showBorrowedBooks() const {
        cout << "   Borrowed Book IDs: ";
        if (borrowedBookIds.empty()) cout << "None";
        else for (int id : borrowedBookIds) cout << id << " ";
        cout << endl;
    }
};

class Librarian : public User {
public:
    Librarian(int id, string n) : User(id, n, "Librarian") {}
    string serialize() const override { return "L|" + to_string(userId) + "|" + name; }
};

// ==========================================
// 4. DB Manager Class
// ==========================================
class DBManager {
public: 
    sqlite3* db;
    char* errMsg;

    string getString(string sql) {
        sqlite3_stmt* stmt;
        string result = "";
        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
        if (rc == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char* text = sqlite3_column_text(stmt, 0);
                if (text) result = string(reinterpret_cast<const char*>(text));
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }
    //Used for queries like COUNT(*).
    int getScalar(string sql) {
        sqlite3_stmt* stmt;
        int rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, 0);
        int result = -1;
        if (rc == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                result = sqlite3_column_int(stmt, 0);
            }
        }
        sqlite3_finalize(stmt);
        return result;
    }

public:
    DBManager() {
        int rc = sqlite3_open("library_data.db", &db);
        if (rc) {
            throw LibraryException("Can't open database: " + string(sqlite3_errmsg(db)));
        }
        initializeTables();
    }

    ~DBManager() {
        sqlite3_close(db);
    }
    //Used for CREATE, INSERT, UPDATE, DELETE
    void execute(string sql) {
        int rc = sqlite3_exec(db, sql.c_str(), 0, 0, &errMsg);
        if (rc != SQLITE_OK) {
            string err = errMsg;
            sqlite3_free(errMsg);
            throw LibraryException("SQL Error: " + err);
        }
    }
    //Executes a query that returns multiple rows.
    // FIXED: Added 'void* data' parameter to pass the vector pointer correctly
    void query(string sql, int (*callback)(void*, int, char**, char**), void* data) {
        int rc = sqlite3_exec(db, sql.c_str(), callback, data, &errMsg);
        if (rc != SQLITE_OK) {
            string err = errMsg;
            sqlite3_free(errMsg);
            throw LibraryException("Query Error: " + err);
        }
    }

    void initializeTables() {
        string usersTable = "CREATE TABLE IF NOT EXISTS USERS(ID INT PRIMARY KEY, NAME TEXT NOT NULL, ROLE TEXT NOT NULL);";
        string booksTable = "CREATE TABLE IF NOT EXISTS BOOKS(ID INT PRIMARY KEY, TITLE TEXT NOT NULL, AUTHOR TEXT NOT NULL, COPIES INT NOT NULL);";
        string transTable = "CREATE TABLE IF NOT EXISTS TRANSACTIONS(USER_ID INT, BOOK_ID INT, PRIMARY KEY (USER_ID, BOOK_ID));";
        execute(usersTable);
        execute(booksTable);
        execute(transTable);
    }

    bool exists(string table, int id) {
        string sql = "SELECT COUNT(*) FROM " + table + " WHERE ID = " + to_string(id) + ";";
        return getScalar(sql) > 0;
    }

    bool isBookBorrowed(int userId, int bookId) {
        string sql = "SELECT COUNT(*) FROM TRANSACTIONS WHERE USER_ID=" + to_string(userId) + " AND BOOK_ID=" + to_string(bookId) + ";";
        return getScalar(sql) > 0;
    }
};

// ==========================================
// 5. Library System
// ==========================================

class LibrarySystem {
private:
    DBManager db; 
    vector<Book> books;
    vector<User*> users; 

    static int loadBookCallback(void* data, int argc, char** argv, char** azColName) {
        vector<Book>* bookList = static_cast<vector<Book>*>(data);
        if (argc >= 4 && bookList != nullptr) {
            try {
                bookList->emplace_back(stoi(argv[0]), argv[1], argv[2], stoi(argv[3]));
            } catch (...) { } 
        }
        return 0;
    }

    Book* findBook(int bookId) {
        for (auto &book : books) if (book.getId() == bookId) return &book;
        return nullptr;
    }

    User* findUser(int userId) {
        for (auto user : users) if (user->getId() == userId) return user;
        return nullptr;
    }

public:
    LibrarySystem() {
        loadBooks();
        loadUsers();
    }

    ~LibrarySystem() {
        for (auto user : users) delete user;
    }

    void saveBooks() { }
    void saveUsers() { }

    void loadBooks() {
        books.clear();
        
        db.query("SELECT * FROM BOOKS;", loadBookCallback, &books);
    }

    void loadUsers() {
        for (auto user : users) delete user; 
        users.clear();
        
        string sql = "SELECT ID, NAME, ROLE FROM USERS;";
        
        sqlite3_stmt* stmt;
        if (sqlite3_prepare_v2(db.db, sql.c_str(), -1, &stmt, 0) == SQLITE_OK) {
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                int id = sqlite3_column_int(stmt, 0);
                string name = string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)));
                string role = string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)));

                if (role == "Librarian") {
                    users.push_back(new Librarian(id, name));
                } else if (role == "Student") {
                    Student* s = new Student(id, name);
                    sqlite3_stmt* trans_stmt;
                    string trans_sql = "SELECT BOOK_ID FROM TRANSACTIONS WHERE USER_ID=" + to_string(id) + ";";
                    if (sqlite3_prepare_v2(db.db, trans_sql.c_str(), -1, &trans_stmt, 0) == SQLITE_OK) {
                        while (sqlite3_step(trans_stmt) == SQLITE_ROW) {
                            s->borrowBook(sqlite3_column_int(trans_stmt, 0));
                        }
                    }
                    sqlite3_finalize(trans_stmt);
                    users.push_back(s);
                }
            }
        }
        sqlite3_finalize(stmt);
    }

    void addStudent(int id, string name) {
        if (db.exists("USERS", id)) { cout << "User ID already exists!" << endl; return; }
        db.execute("INSERT INTO USERS (ID, NAME, ROLE) VALUES (" + to_string(id) + ", '" + name + "', 'Student');");
        users.push_back(new Student(id, name));
    }

    void addLibrarian(int id, string name) {
        if (db.exists("USERS", id)) { cout << "User ID already exists!" << endl; return; }
        db.execute("INSERT INTO USERS (ID, NAME, ROLE) VALUES (" + to_string(id) + ", '" + name + "', 'Librarian');");
        users.push_back(new Librarian(id, name));
    }

    void addBook(int userId, int bookId, string title, string author, int copies) {
        User* user = findUser(userId);
        if (!user || user->getRole() != "Librarian") throw PermissionDeniedException();
        
        if (db.exists("BOOKS", bookId)) {
            string existingTitle = db.getString("SELECT TITLE FROM BOOKS WHERE ID = " + to_string(bookId));
            
            if (existingTitle == title) {
                db.execute("UPDATE BOOKS SET COPIES = COPIES + " + to_string(copies) + " WHERE ID = " + to_string(bookId) + ";");
                cout << "Success: Book matched (ID & Title). Copies increased." << endl;
            } else {
                // MISMATCH: Same ID but Different Title -> Error
                cout << "Error: Conflict! Book ID " << bookId << " is already assigned to '" << existingTitle << "'." << endl;
                cout << "You cannot add '" << title << "' with this ID." << endl;
                return; 
            }
        } else {
            string sql = "INSERT INTO BOOKS (ID, TITLE, AUTHOR, COPIES) VALUES (" 
                       + to_string(bookId) + ", '" + title + "', '" + author + "', " + to_string(copies) + ");";
            db.execute(sql);
            cout << "Success: New book added to library." << endl;
        }
        
        loadBooks(); 
    }

    void borrowBook(int userId, int bookId) {
        User* user = findUser(userId);
        Book* book = findBook(bookId); 

        if (!user) throw InvalidUserException();
        if (!book) throw InvalidBookException();

        int copies = db.getScalar("SELECT COPIES FROM BOOKS WHERE ID = " + to_string(bookId));
        if (copies <= 0) throw BookNotAvailableException();
        if (db.isBookBorrowed(userId, bookId)) { cout << "Student already has this book." << endl; return; }

        Student* student = dynamic_cast<Student*>(user);
        if (student) {
            db.execute("INSERT INTO TRANSACTIONS (USER_ID, BOOK_ID) VALUES (" + to_string(userId) + ", " + to_string(bookId) + ");");
            db.execute("UPDATE BOOKS SET COPIES = COPIES - 1 WHERE ID = " + to_string(bookId) + ";");
            
            student->borrowBook(bookId);
            book->decreaseCopy(); 

            cout << "Borrowed: " << book->getTitle() << endl;
        } else {
            cout << "Only students can borrow." << endl;
        }
    }

    void returnBook(int userId, int bookId) {
        User* user = findUser(userId);
        Book* book = findBook(bookId); 

        if (!user) throw InvalidUserException();
        if (!book) throw InvalidBookException();
        
        if (!db.isBookBorrowed(userId, bookId)) {
            cout << "Student does not have this book." << endl;
            return;
        }

        Student* student = dynamic_cast<Student*>(user);
        if (student) {
            db.execute("DELETE FROM TRANSACTIONS WHERE USER_ID=" + to_string(userId) + " AND BOOK_ID=" + to_string(bookId) + ";");
            db.execute("UPDATE BOOKS SET COPIES = COPIES + 1 WHERE ID = " + to_string(bookId) + ";");

            student->returnBook(bookId);
            book->increaseCopy(); 

            cout << "Returned: " << book->getTitle() << endl;
        }
    }

    void displayAll() {
        loadBooks();
        loadUsers(); 

        cout << "\n=== CURRENT LIBRARY STATE (From DB) ===" << endl;
        cout << "--- Books ---" << endl;
        if(books.empty()) cout << "No books in library." << endl;
        for (const auto &book : books) book.display();
        
        cout << "\n--- Users ---" << endl;
        if(users.empty()) cout << "No registered users." << endl;
        for (const auto user : users) {
            user->display();
            Student* s = dynamic_cast<Student*>(user);
            if(s) s->showBorrowedBooks();
        }
        cout << "=============================\n" << endl;
    }
};

// ==========================================
// 6. Main Driver
// ==========================================

int main() {
    LibrarySystem library;
    
    int choice;
    while(true) {
        cout << "1. Add Librarian\n2. Add Student\n3. Add Book (as Lib)\n4. Borrow Book\n5. Return Book\n6. Display All\n7. Exit\nChoice: ";
        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            continue;
        }

        try {
            if (choice == 1) {
                int id; string name;
                cout << "Enter ID: "; cin >> id ;
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Enter Name: "; getline(cin,name);
                library.addLibrarian(id, name);
            }
            else if (choice == 2) {
                int id; string name;
                cout << "Enter ID: "; cin >> id ;
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                cout << "Enter Name: "; getline(cin,name);
                library.addStudent(id, name);
            }
            else if (choice == 3) {
                int uid, bid, copies; string title, author;
                cout << "Librarian ID: "; cin >> uid;
                cout << "Book ID: "; cin >> bid;
                cout << "Copies: "; cin >> copies;
                cin.ignore();
                cout << "Title: "; getline(cin, title);
                cout << "Author: "; getline(cin, author);
                library.addBook(uid, bid, title, author, copies);
            }
            else if (choice == 4) {
                int uid, bid;
                cout << "Student ID & Book ID: "; cin >> uid >> bid;
                library.borrowBook(uid, bid);
            }
            else if (choice == 5) {
                int uid, bid;
                cout << "Student ID & Book ID: "; cin >> uid >> bid;
                library.returnBook(uid, bid);
            }
            else if (choice == 6) {
                library.displayAll();
            }
            else if (choice == 7) {
                break;
            }
        } catch (exception& e) {
            cout << "Error: " << e.what() << endl;
        }
    }

    return 0;
}