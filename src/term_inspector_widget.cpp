#include "aiws/gui/term_inspector_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
namespace aiws::gui {

TermInspectorWidget::TermInspectorWidget(QWidget* parent) : QWidget(parent) {
    // TODO: Construct and lay out the Term Inspector, connect local events,
    // and establish initial state.
    //Setting up the main vertical layout 
    QVBoxLayout* main_lay = new QVBoxLayout(this);
    //Setting up section heading
    QLabel* ti_title = new QLabel("Term Inspector", this);
    main_lay->addWidget(ti_title);
    //Setting up input controls
    QHBoxLayout* input_lay = new QHBoxLayout();
    term_edit_ = new QLineEdit(this);
    term_edit_->setPlaceholderText("Enter a term:");
    inspect_button_ = new QPushButton("Inspect", this);
    inspect_button_->setEnabled(false);
    input_lay->addWidget(new QLabel("Term:", this));
    input_lay->addWidget(term_edit_, 1);
    input_lay->addWidget(inspect_button_);
    main_lay->addLayout(input_lay);
    //Adding frequency stats
    QGridLayout* stats_lay = new QGridLayout();
    QLabel*  df_label = new QLabel("Document Frequency:",this);
    QLabel* tf_label = new QLabel("Selected Chunk TF:",this);
    document_frequency_value_ = new QLabel("-", this);
    chunk_frequency_value_ = new QLabel("-", this);
    stats_lay->addWidget(df_label, 0, 0);
    stats_lay->addWidget(document_frequency_value_, 0, 1);
    stats_lay->addWidget(tf_label, 1, 0);
    stats_lay->addWidget(chunk_frequency_value_, 1, 1);
    main_lay->addLayout(stats_lay);
    main_lay->addStretch();
    //Connecting the inspect button to private slot
    connect(inspect_button_, &QPushButton::clicked, this, &TermInspectorWidget::request_inspection);
    //Connecting and editing the term invalidates displayed stats
    connect(term_edit_, &QLineEdit::textChanged, this, &TermInspectorWidget::clear_statistics);

}

void TermInspectorWidget::set_inspection_enabled(bool enabled) {
   inspect_button_->setEnabled(enabled);
}

void TermInspectorWidget::set_selected_chunk(const QString& chunk_id) {
    //Function used to detect change in the search result (selected) that would 
    //invalidate the previous chunk's TF, and not the DF. 
    //Only calculates new TF when Inspect button is clicked
   if(selected_chunk_id_ != chunk_id){
    selected_chunk_id_ = chunk_id;
    chunk_frequency_value_->setText("-"); //the older frequency no longer applies
   }
}

void TermInspectorWidget::set_statistics(std::size_t document_frequency,
                                         bool has_chunk_frequency,
                                         std::size_t chunk_frequency) {
   //always display the document frequency
   document_frequency_value_->setText(QString::number(static_cast<qulonglong>(document_frequency)));
   //Display the tf if a chunk was selected
   if(has_chunk_frequency){
    chunk_frequency_value_->setText(QString::number(static_cast<qulonglong>(chunk_frequency)));
   } else {
    chunk_frequency_value_->setText("-");
   }
}

void TermInspectorWidget::clear_statistics() {
    //resets displayed stats
    document_frequency_value_->setText("-");
    chunk_frequency_value_->setText("-");
}

void TermInspectorWidget::reset() {
    // fuffills resettable interface for Milestone 3
    term_edit_->clear();
    selected_chunk_id_.clear();
    clear_statistics();
    set_inspection_enabled(false);
}

void TermInspectorWidget::request_inspection() {
    //Inspection has to be enabled
    if(!inspect_button_->isEnabled()){
        return;
    }
    QString term = term_edit_->text().trimmed();
    if(term.isEmpty()){ //ignores the empty or whitespace-only input
        return;
    }
    //Requests a backend inspection from the MainWindow
    term_inspection_requested(term, selected_chunk_id_); //emit
}

}  // namespace aiws::gui
