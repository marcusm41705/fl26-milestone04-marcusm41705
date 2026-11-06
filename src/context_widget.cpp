#include "aiws/gui/context_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QStringList>

namespace aiws::gui {

ContextWidget::ContextWidget(QWidget* parent) : QWidget(parent) {
    
    //setting up main vertical layout
    QVBoxLayout* main_lay = new QVBoxLayout(this);
    //Setting up section title
    QLabel* sec_title = new QLabel("Context", this);
    main_lay->addWidget(sec_title);
    //Setting up budget controls
    QHBoxLayout* budget_lay = new QHBoxLayout();
    QLabel* budget_label = new QLabel("Token Budget:", this);
    //Setting up horizontal budget slider
    context_budget_slider_ = new QSlider(Qt::Horizontal, this);
    context_budget_slider_->setRange(50, 1000);
    context_budget_slider_->setSingleStep(50);
    context_budget_slider_->setPageStep(50);
    context_budget_slider_->setTickInterval(50);
    context_budget_slider_->setValue(300);
    //Displays the current budget
    budget_value_label_ = new QLabel("300", this);
    //setting up build context button
    build_context_button_ = new QPushButton("Build Context", this);
    build_context_button_->setEnabled(false);
//arranging the controls for budget
    budget_lay-> addWidget(budget_label);
    budget_lay->addWidget(context_budget_slider_, 1);
    budget_lay->addWidget(budget_value_label_);
    budget_lay->addWidget(build_context_button_);
    main_lay->addLayout(budget_lay);

    //context display (read only)
    context_text_ = new QPlainTextEdit(this);
    context_text_->setReadOnly(true);
    context_text_->setPlaceholderText("Build context to display the matching content.");
    main_lay->addWidget(context_text_, 1);
    //Connecting the slider to the value update slot
    connect(context_budget_slider_, &QSlider::valueChanged, this, &ContextWidget::budget_changed);
    //Connecting the button to request a slot
    connect(build_context_button_, &QPushButton::clicked, this,&ContextWidget::request_context);

}

int ContextWidget::token_budget() const {
    
    return context_budget_slider_->value();
}

void ContextWidget::set_build_context_enabled(bool enabled) {
    build_context_button_->setEnabled(enabled);
}

void ContextWidget::set_context(const std::vector<ContextItem>& items) {
    //This function presents the context items as returned by the backend. 
    //Values like token counts, truncation flagging, and scores are results
    //from the backend, so the GUI does not recalculate / modify these. 
    QStringList context_blocks;
    //Formatting for each context item
    for (const ContextItem& item: items){
        QString block = QString("Document: %1\n" 
            "Chunk: %2\n" 
            "Tokens: %3\n"
             "Score: %4\n" "Truncated: %5\n" "Text:\n%6")
            .arg(QString::fromStdString(item.document_id))
            .arg(static_cast<qulonglong>(item.chunk_sequence))
            .arg(static_cast<qulonglong>(item.token_count))
            .arg(QString::number(item.score, 'g', 12))
            .arg(item.truncated ? "Yes" : "No")
            .arg(QString::fromStdString(item.text));
            context_blocks.append(block);

    }
    context_text_->setPlainText(context_blocks.join("\n\n---------------\n\n"));
}

void ContextWidget::clear_context() {
    context_text_->clear();

}

void ContextWidget::reset() {
    context_budget_slider_->setValue(300);
    clear_context();
    set_build_context_enabled(false);
}

void ContextWidget::request_context() {
    //This function emits the selected token budget without 
    // constructing the context. 

    //ensuring the action is enabled
    if(!build_context_button_->isEnabled()){
        return;
    }
    //notify the MainWindow
    build_context_requested(token_budget()); //emit
}

void ContextWidget::budget_changed(int value) {
    int rounded = ((value + 25)/50) * 50; //Budget to nearest 50
    if(rounded != value){
        context_budget_slider_->setValue(rounded);
        return;
    }
    //updates the budget display
    budget_value_label_->setText(QString::number(value));
}

}  // namespace aiws::gui
