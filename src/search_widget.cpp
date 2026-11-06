#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/search_result_model.hpp"
#include <QAbstractItemView>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <utility>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTableView>
namespace aiws::gui {

SearchWidget::SearchWidget(QWidget* parent) : QWidget(parent) {
    //Construct and lay out the Query, Search Results, and Selected Result
    // areas; configure the table/model; connect local events; establish initial state.
    //Initializing veritical main layout
    auto* main_lay = new QVBoxLayout(this);
    auto* search_title = new QLabel("Query / Search", this);
    main_lay->addWidget(search_title);
    //Setting up controls for query
    auto* query_lay = new QHBoxLayout();
    query_edit_ = new QLineEdit(this);
    query_edit_->setPlaceholderText("Enter search query...");
    //Result count spin box
    result_count_spin_ = new QSpinBox(this);
    result_count_spin_->setRange(1, 100);
    result_count_spin_->setValue(3);
    //Search button
    search_button_ =  new QPushButton("Search", this);
    search_button_->setEnabled(false);
    //query layout
    query_lay->addWidget(new QLabel("Query:", this));
    query_lay->addWidget(query_edit_, 1);
    query_lay->addWidget(new QLabel("Results:", this));
    query_lay->addWidget(result_count_spin_);
    query_lay->addWidget(search_button_);
    main_lay->addLayout(query_lay);
    //Search result table + model
    result_model_ = new SearchResultModel(this);
    results_view_ = new QTableView(this);
    results_view_->setModel(result_model_);
    results_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_view_->horizontalHeader()->setStretchLastSection(true);
    main_lay->addWidget(results_view_,2);
    //selected result details
    auto* selected_label = new QLabel("Selected Result", this);
    selected_result_text_ = new QPlainTextEdit(this);
    selected_result_text_->setReadOnly(true);
    main_lay->addWidget(selected_label);
    main_lay->addWidget(selected_result_text_, 1);
    //View the source button
    view_source_button_ = new QPushButton("View Source", this);
    view_source_button_->setEnabled(false);
    main_lay->addWidget(view_source_button_);
    //Signal + slot connections
    connect(query_edit_, &QLineEdit::textChanged, this, &SearchWidget::query_changed);
    connect(search_button_, &QPushButton::clicked, this, &SearchWidget::request_search);
    connect(view_source_button_, &QPushButton::clicked, this, &SearchWidget::request_source_document);
    connect(results_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SearchWidget::show_selected_result);
}

QString SearchWidget::query() const {
    return query_edit_->text();
}

int SearchWidget::result_count() const {
    
    return result_count_spin_->value();
}

bool SearchWidget::has_query() const {
    return (!query_edit_->text().trimmed().isEmpty());
}

void SearchWidget::set_search_enabled(bool enabled) {
   search_button_->setEnabled(enabled);
}

void SearchWidget::set_results(std::vector<SearchResult> results) {
    //Notify the attached Qt views before and after the replacement of the model
    //Ensures the results table will refresh correctly
    results_view_->clearSelection(); //Removes the previous selection
    //Replaces the models results
    result_model_->set_results(std::move(results));
    //Clear the result dependent state
    selected_result_text_->clear();
    view_source_button_->setEnabled(false);
     selected_chunk_changed(QString()); //emit 
}

void SearchWidget::clear_results() {
    results_view_->clearSelection();
    result_model_->clear();
    selected_result_text_->clear();
    view_source_button_->setEnabled(false);
    selected_chunk_changed(QString()); //emit
}

void SearchWidget::reset() {
    query_edit_->clear();
    result_count_spin_->setValue(3);
    clear_results();
    set_search_enabled(false);
}

void SearchWidget::request_search() {
    //Emits the user query and requested result count so 
    // that Main Window can invoke the existing Processing Core backend.
    if(!search_button_->isEnabled()  || !has_query()){
        return;
    }
    search_requested(query(), result_count()); //emit
}

void SearchWidget::show_selected_result() {
    //Function used to forward the selected chunk to Term Inspector Widget
    //so that the TF is able to be calculated when requested. 
    //Determines what row of the table is selected
    const QModelIndexList select = results_view_->selectionModel()->selectedRows();
    if(select.isEmpty()){ //if no valid selection
        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
        selected_chunk_changed(QString()); //emit
        return;
    }
    int row = select.first().row();
    const SearchResult* res = result_model_->result_at(row);
    if(res == nullptr){
        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
        selected_chunk_changed(QString()); //emit
        return;
    }
    QString details = QString("Document: %1\n" 
    "Chunk: %2\n" "Score: %3\n" "Text:\n%4").arg(QString::fromStdString(res->document_id))
    .arg(static_cast<qulonglong>(res->chunk_sequence))
    .arg(res->score)
    .arg(QString::fromStdString(res->text));
    selected_result_text_->setPlainText(details);
    view_source_button_->setEnabled(true); //valid selection will allow to view the source
    //Notifies the inspector about selected chunk
    selected_chunk_changed(QString::fromStdString(res->chunk_id)); //emit
}

void SearchWidget::request_source_document() {
   const QModelIndexList select = results_view_->selectionModel()->selectedRows();
   if(select.isEmpty()){
    return;
   }
   const SearchResult* res = result_model_->result_at(select.first().row());
   if(res == nullptr){
    return;
   }
    source_document_requested(QString::fromStdString(res->document_id));
   
}

}  // namespace aiws::gui
