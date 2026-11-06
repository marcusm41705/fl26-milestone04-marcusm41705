#include "aiws/gui/search_result_model.hpp"

namespace aiws::gui {

SearchResultModel::SearchResultModel(QObject* parent)
    : QAbstractTableModel(parent) {
    // TODO
}

int SearchResultModel::rowCount(const QModelIndex& parent) const {
    if(parent.isValid()){
        return 0;
    }
    return static_cast<int>(results_.size());
}

int SearchResultModel::columnCount(const QModelIndex& parent) const {
    if(parent.isValid()){
        return 0;
    }
    return 5;
}

QVariant SearchResultModel::data(const QModelIndex& index, int role) const {
    //Used when Qt requests individual cell values
    if(!index.isValid() || role != Qt::DisplayRole){
        return {};
    }
    const SearchResult* result = result_at(index.row());
    if(result == nullptr){
        return{};
    }
    switch(index.column()){
        case 0:
        return index.row() + 1;
        //break;
        case 1: 
        return QString::fromStdString(result->document_id);
        //break;
        case 2: 
        return QVariant::fromValue(static_cast<qulonglong>(result->chunk_sequence));
        //break;
        case 3:
        return result->score;
        //break;
        case 4:
        return QString::fromStdString(result->text);
        //break;
        default: return {};
        //break;
    }
}

QVariant SearchResultModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const {
    if(role != Qt::DisplayRole || orientation != Qt::Horizontal){
        return{};
    }
    switch(section){
        case 0:
        return "Rank";
        case 1:
        return "Document";
        case 2:
        return "Chunk";
        case 3:
        return "Score";
        case 4: 
        return "Text";
        default:
        return {};
    }
}

void SearchResultModel::set_results(std::vector<SearchResult> results) {
//Notify the attached Qt views before and after the replacement of the model
    //Ensures the results table will refresh correctly    beginResetModel();
    results_ = std::move(results);
    endResetModel();
}

void SearchResultModel::clear() {
    beginResetModel();
    results_.clear();
    endResetModel();
}

const SearchResult* SearchResultModel::result_at(int row) const noexcept {
    //Function retrieves result at specific row
   if(row < 0 || static_cast<std::size_t>(row) >= results_.size()){
    return nullptr;
   }
   return &results_[static_cast<std::size_t>(row)];
}

}  // namespace aiws::gui
