# M4 Decision Record

See specification Section 8.

1. Threads and Ownership
For Milestone 4, the implementation uses the GUI thread, a background worker used to build the background corpus and a separate IPC listener thread. The GUI thread owns the elements of MainWindow, Workspace, the accepted ProcessingCore, and all of the widgets used in the Application. The function MainWindow::build_corpus() is used to create a copy of the current Workspace, then launches a background task that uses std::async. The background worker constructs a new instance of ProcessingCore using the function ProcessingCore::rebuild() without any modification to the GUI. QTimer calls MainWindow::check_build_fin() and checks std::future for completion. When this is completed, ownership of the new corpus gets transferred to MainWindow is the result. In CR-1, IPCServer owns the std::thread reponsible for listening to incoming socket connections, and the listener recieves requests and then forwards accepted document paths using QMetaObject::invokeMethod() with the Qt:QueuedConnection. This ensures that the function MainWindow::add_document_file() executes using the GUI thread. 

Shared Mutable Data:
IPC Server's stop_requested_flag is accessed by both the GUI thread and the listener thread. The GUI thread will write to the flag during the application shutdown, while the listener will read it and determine when to stop. This gets declared as a std::atomic<bool> and prevents a data race. The listening file descriptor listen_fd_ is initialized before the listener starts and is read by the listener while it runs and then closed only after worker_.join(). Therefore, it is not modified concurrently. The Workspace, widgets, and the accepted ProcessingCore remain accessible for modification only on the GUI thread. Then, the background worker uses it's own copy of the Workspace and constructs the ProcessingCore. The shared result of the std::future is then synchronized using the C++ standard library. Detatched threads are unused and the worker is completed before the user can exit the application, then IPCServer::stop() joins the listener thread before the server is destroyed. 

2. Build Validation

The MainWindow::build_corpus function is used to record the current workspace_revision_ as build_revision_ when the build begins. When a document is added or Workspace is reset, the current Workspace revision changes and the corpus that was previously accepted becomes invalidated. When MainWindow::check_build_fin() gets the completed result, it then compares build_revision_ against the  workspace_revision_. If the revisions match, the fully completed ProcessingCore then becomes the current corpus and the application will enter a Ready state. When the revisions are different, the completed result gets discarded because it no longer represents the current Workspace. If an exception is thrown, handle_build_failure() reports this failure then allows the application to be able to recover, and not remain permanently in the Building state.
This will preserve the requirement that Search, Context, and Term Inspector can use the corpus that corresponds to the current workspace. 

3. Changes Since Design Checkpoint
The original design checkpoint in DESIGN_NOTE.md had established four states for the corpus: No Current Corpus, Building, Ready, and Failed. It also identified the risks that GUI blocking and outdated corpuses can present. This implementation finalized the asynchronous corpus building using std::future, QTimer, and std::async. A new change was adding revision checking when a build is completed, along with exception handling for any failed builds and lifecycle handling for confirmed exit requests. 

4. CR-1 Impact
CR-1 required multiple changes to the application entry points, along with document-addition integration and configurating builds. New components included tools/aiws.cpp, which implemented the command-line parsing, absolute path resolution, Unix socket connections, transmission requesting, response handling, and deadlines for communication. IPCServer::start(), IPCServer functions like start(), run(), handle_client(), and stop() were implemented to manage responsibilities such as any incoming connections, recieving document requests, file validation, response sending, and cleaning up the listening socket after an exit. The main() function was changed to accept --ipc-socket, create the IPC Server, then forward requests to the GUI, and stops the listener during application shutdown. MainWindow::add_document_file() was used to provide the document-addition path that is used by both the GUI and remote requests. CMakeLists.txt was also edited to include the source files used to implement the server and the executable for aiws_send. 

For CR-1's message format, the client sends a four-byte unsigned path length formatted in big-endian order, which is followed by the number of UTF-8 path bytes. The server then responds with a one byte status: 0 for accepted, 1 for rejected. The server also responds with a four-byte big-endian reason length and corresponding UTF-8 reason bytes. The implementation uses loops built around send() and recv() as a Unix stream socket can only transfer part of a message during the operation. 

Components that did not change during the implementation includes the Workspace, Document, and ProcessingCore interfaces as remote additions are able to use the same corpus invalidation and document processing paths as the GUI additions, which preserves the existing Milestone 3 functionality and M4 checks for correctness. 

5. Fault Switches and Tests
The AIWS_FAULT_ACCEPT_STALE fault is located inside MainWindow::check_build_fin(), where the application decides whether the completed corpus will match the revision to the current Workspace. When this fault is enabled the application will accept a stale build, even after the addition of a document or the reset of the Workspace. 
The AIWS_FAULT_LOSE_FAILURE fault is located in MainWindow::handle_build_fail(), where the application manages any failures with  background corpus-building. When this fault is enabled, the failure isn't reported to the GUI, leaving the application in the Building state. All of the normal created tests in student_tests have passed, and all of the corresponding fault-enabled configurations have expectedly failed. 




##Pair Programming Log
Partner Name: Leo Dong

Session 1: November 7, 2026, 3:00 pm to 3:40 pm, working on my code. 

Driver: Marcus (3:00-3:22 pm) Leo (3:23-3:40pm)
Navigator: Leo (3:00-3:22 pm) Marcus (3:23-3:40pm)

As the driver, we worked on the implementation of the remote document addition for CR-1. Through the first driver-navigator pair I worked on the command-line client, which included working through developing argument parsing and resolving the absolute file paths. After 20 minutes, we swapped roles while still continuing the work on my implementation. 

A contribution I made as driver was the full implementation of forwarding accepted requests to the GUI thread, which allowed the Workspace to process any remotely added documents. A contribution that I made as navigator was to review the implementation for potential errors, including errors with compilation and mistakes inside the code. I was able to provide verbal feedback to Leo as we worked through the implementation. 

The peer programming method that worked best for us was having the current Navigator responsible for testing, go through the application and ensure that the document was added remotely using the command line. This allowed the driver to be able to focus completely on the implementation side, while the navigator was able to focus completely on the functionality side. 

Session 2: November 7, 2026, 3:45 pm - 4:25 pm, working on Leo's code. 
Driver: Leo (3:45-4:09 pm) Marcus (4:10-4:25pm)
Navigator: Marcus (3:45-4:09 pm) Leo (4:10-4:25pm)

We worked primarily on the CR1 message framing and remote-add integration using Leo's implementation. 

As the driver, I started by using one call to recv(), but Leo, as the navigator gave me feedback that one recv() call might not be enough to contain the entire path data and message. In response, I was able to implement a loop that that was able to continously receive all bytes of data. As the navigator, I suggested that Leo should implement the function add_document_file in order to accept remote requests through routing, which allowed any remote additions to preserve the existing workspace and behavior associated with corpus invalidation.

 We used the driver/navigator pair method to approach implementation decisions for Leo's code, that focused on communication between the GUI thread and IPC listener thread, specifically on how Leo can receive complete messages and safely forward any additions to the documents. Switching roles had allowed us to contribute to Leo's implementation either through writing code or reviewing implementation decisions. 

