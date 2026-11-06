#include "aiws/gui/workspace_widget.hpp"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
namespace aiws::gui {

WorkspaceWidget::WorkspaceWidget(QWidget* parent) : QWidget(parent) {
    // TODO: Construct and lay out the Workspace and Selected Document areas,
    // connect local widget events, and establish initial presentation state.
    auto* main_lay = new QHBoxLayout(this); //The main horizontal layout
    //Workspace on left side
    auto* left = new QWidget(this);
    auto left_lay = new QVBoxLayout(left);
    auto* wp_label = new QLabel("Workspace", left);
    //Initialize document and build corpus buttons
    add_document_button_ = new QPushButton("Add Document", left);
    build_corpus_button_ = new QPushButton("Build Corpus", left);
    document_list_ = new QListWidget(left);
    corpus_status_label_ = new QLabel("Corpus: Not built", left);
    //New layout for buttons
    auto* button_layout = new QHBoxLayout();
    button_layout->addWidget(add_document_button_);
    button_layout->addWidget(build_corpus_button_);

    left_lay->addWidget(wp_label); //Workspace label
    left_lay->addLayout(button_layout); //Button layout 
    left_lay->addWidget(document_list_); //Document list
    left_lay->addWidget(corpus_status_label_); //Label for corpus status 

    //Layout from right side: selected document
    auto* right = new QWidget(this);
    auto right_lay = new QVBoxLayout(right);
    document_title_label_ = new QLabel("Selected Document -", right);
    document_text_ = new QPlainTextEdit(right);
    document_text_->setReadOnly(true);
    document_text_->setPlaceholderText("Select the document to view it's contents.");
    right_lay->addWidget(document_title_label_);
    right_lay->addWidget(document_text_ );
    //Put both sections together
    main_lay->addWidget(left, 1);
    main_lay->addWidget(right, 1);
    //Initializing the button state
    build_corpus_button_->setEnabled(false);
    //Connecting the buttons to the component values
    //connect document elements
    connect(add_document_button_, &QPushButton::clicked,this,&WorkspaceWidget::add_document_requested);
    //connect corpus elements
    connect(build_corpus_button_, &QPushButton::clicked, this, &WorkspaceWidget::build_corpus_requested);
    //connect document selection elements
    connect(document_list_, &QListWidget::currentRowChanged, this, &WorkspaceWidget::document_selected);

}

void WorkspaceWidget::add_document(const QString& title) {
    //Adds the document selected from the document list
    document_list_->addItem(title);
}

void WorkspaceWidget::select_document(int row) {
    //Sets the current row to the selected document from the parameter row
    document_list_->setCurrentRow(row);
}

void WorkspaceWidget::show_selected_document(const QString& title,
                                              const QString& text) {
                                                //Show the title and text of the selected document
    document_title_label_->setText(QString("Selected Document - %1").arg(title));
    document_text_->setPlainText(text);
}

void WorkspaceWidget::clear_selected_document() {
    //Clear the text of the selected document
    document_title_label_->setText("Selected Document -");
    document_text_->clear();
}

void WorkspaceWidget::set_corpus_status(const QString& text) {
    //update the text of the corpus status label
    corpus_status_label_->setText(text);
}

void WorkspaceWidget::set_build_corpus_enabled(bool enabled) {
    //enables the button used to build the corpus
    build_corpus_button_->setEnabled(enabled);
}

void WorkspaceWidget::reset() {
    //resets the document list, selected document, corpus status, and corpus build enable 
    document_list_->clear();
    clear_selected_document();
    set_corpus_status("Corpus: Not Built");
    set_build_corpus_enabled(false);
}

}  // namespace aiws::gui
