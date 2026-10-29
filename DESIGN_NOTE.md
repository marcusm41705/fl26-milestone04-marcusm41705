# M4 Design Note

Due Wed Oct 28, 11:59 pm. See specification Section 7.
Author: Marcus Mason
Components:
The components in Milestone 4 are extended from the Milestone 3 AI workspace. The first component is MainWindow, which handles user input, updates the GUI, and determines whether a fully completed corpus still matches the current Workspace. The thread MainWindow runs on is the GUI thread. 
Workspace maintains any current documents inside the application and runs on the GUI thread. 
WorkspaceWidget is used to display both the current documents and the status of the corpus inside the application, which also runs on the GUI thread. ProcessingCore is responsible for chunking, indexing, and retrieval purposes, and in Milestone 4, it will run separately on the background worker during building and on the GUI for accepted corpus operations. The background worker is to be implemented during Milestone 4’s complete development, built using standard C++, and will be used to build new corpuses containing the correct documents.
 
Corpus States:
The application will track the validity of a corpus through different states. The first state is “No Current Corpus”, which means that there is currently no valid corpus that is available for the current Workspace. 
The state “Building” means that the background worker is currently constructing the corpus. The state “Ready” means that a completed corpus has passed the validation process and can be searched. Lastly, the “Failed” state means that the build has failed, but the user can attempt to build the corpus again. 
The application transitions from “No Current Corpus” to “Building” whenever the user requests Build Corpus. If the build is completed successfully and matches the current Workspace, the state changes from “Building” to “Ready”. If this build fails, then the state changes from “Building” to “Failed”. If the user adds a document while the corpus is being built, then the application will remain in the “Building” state as the corpus finishes building. The resulting corpus will be outdated, then discarded, and the application will return to the “No Current Corpus” state. 

Event Sequences:
A potential risk that would threaten R1’s responsiveness would be the possibility of the GUI freezing during processing. This would stop the user from being able to interact with the application’s elements. A solution to this is to keep the task of corpus building on a background thread, which allows the user to keep interacting with elements such as selecting and adding documents, editing the inputs, and the Reset and Exit buttons. 

A potential risk that threatens R2’s correctness is the possibility of the Workspace changing while a corpus is still being built. This would result in the built corpus providing outdated information to the user. A solution to this is to compare revisions to the Workspace before the result is accepted. 
Step by Step: 
The background worker begins constructing the corpus while using the current Documents inside the Workspace.
While the worker is building, the user adds another document to the Workspace, which changes the Workspace revision. 
The corpus state will remain in “Building” until the background worker finishes processing and building the corpus with the original documents. 
Once the corpus build finishes, MainWindow will compare the original Workspace revision with the current revision. 
The revisions will not match, so the completed corpus is outdated and then discarded. The corpus state will return to the “No Current Corpus” state, which will allow the user to start another build. 

A potential risk that threatens R3’s Failure condition would be the possibility that the worker fails, which would leave the GUI stuck in the “Building” State. This would mean that a failed build would not be reported to the user and may lead to an unusable application. A solution to this is to add a “Failed” state for the corpus status, which would allow the Application to report a failed build to the user and allow them to build another corpus. 

A potential risk that threatens R4’s Lifecycle condition is the possibility that the User exits or resets while the worker is building a corpus. This can cause unsafe conditions and can lead to potential crashes. A solution to this is that, during a reset, the Workspace should immediately be cleared and the current corpus should be invalidated, which would allow the background worker to finish processing the corpus build, which is then discarded. During an exit, any running background worker must be stopped safely, and the thread it runs on must be joined before the owner is destroyed. 
Step by Step:
The current Workspace contains two documents. 
The user clicks the Build Corpus button.
The background worker begins processing and building the corpus. 
The user clicks the Reset button before the corpus build is finished.
MainWindow will immediately clear the Workspace, then invalidate the current corpus, and update the Workspace revision. This allows the background worker to finish processing. 
The background worker will eventually complete the corpus build and will return the result to MainWindow. 
MainWindow will compare the revision recorded at the beginning of the build with this current Workspace revision. 
The revisions will not match, so the finished corpus gets discarded, and the application will remain in the “No Current Corpus” state inside the empty Workspace. 

Ownership Path:
During the creation of a corpus, it is initially owned by the background worker, which is responsible for maintaining a temporary instance of ProcessingCore and building the corpus. Once the build is completed, the result gets transferred to MainWindow, where the Workspace revisions are compared to determine whether the corpus is valid. MainWindow will take ownership of a valid corpus, where the GUI will use it for retrieval. If the corpus is outdated, it gets discarded and destroyed. An accepted corpus will be destroyed when the application exits, Workplace gets reset, or it gets replaced. 

Alternative:
An alternative that was considered for Milestone 4, but ultimately rejected, was the decision to use an instance of ProcessingCore that was shared instead of having to make a separate instance for the background worker. This was rejected because two threads would have to access the same data while one (the background worker) is currently modifying it. User search would return outdated, inconsistent, or incorrect information and could cause the application to crash. Instead, this Milestone 4 design will use a separate ProcessingCore instance for the background worker and allow a completed corpus to be validated before MainWindow accepts ownership. 


