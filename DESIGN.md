# M4 Decision Record

See specification Section 8.

##Pair Programming
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

