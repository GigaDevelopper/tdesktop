/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "history/history_view_highlight_manager.h"

#include "data/data_session.h"
#include "history/history_item.h"
#include "history/view/history_view_element.h"

namespace HistoryView {

ElementHighlighter::ElementHighlighter(
	not_null<Data::Session*> data,
	ViewForItem viewForItem,
	RepaintView repaintView)
: _data(data)
, _viewForItem(std::move(viewForItem))
, _repaintView(std::move(repaintView))
, _timer([=] { updateMessage(); })
, _animation(*this) {
}

void ElementHighlighter::enqueue(not_null<Element*> view) {
	const auto item = view->data();
	const auto fullId = item->fullId();
	if (_queue.empty() && !_animation.animating()) {
		highlight(fullId);
	} else if (_highlightedMessageId != fullId
		&& !base::contains(_queue, fullId)) {
		_queue.push_back(fullId);
		checkNextHighlight();
	}
}

void ElementHighlighter::checkNextHighlight() {
	if (_animation.animating()) {
		return;
	}
	const auto nextHighlight = [&] {
		while (!_queue.empty()) {
			const auto fullId = _queue.front();
			_queue.pop_front();
			if (const auto item = _data->message(fullId)) {
				if (_viewForItem(item)) {
					return fullId;
				}
			}
		}
		return FullMsgId();
	}();
	if (!nextHighlight) {
		return;
	}
	highlight(nextHighlight);
}

float64 ElementHighlighter::progress(
		not_null<const HistoryItem*> item) const {
	if (item->fullId() == _highlightedMessageId) {
		const auto progress = _animation.progress();
		const auto firstPart = st::activeFadeInDuration
			/ float64(st::activeFadeInDuration + st::activeFadeOutDuration);
		return std::min(progress / firstPart, 1.)
			- ((progress - firstPart) / (1. - firstPart));
	}
	return 0.;
}

void ElementHighlighter::highlight(FullMsgId itemId) {
	if (const auto item = _data->message(itemId)) {
		if (const auto view = _viewForItem(item)) {
			_highlightStart = crl::now();
			_highlightedMessageId = itemId;
			_animation.start();

			repaintHighlightedItem(view);
		}
	}
}

void ElementHighlighter::repaintHighlightedItem(
		not_null<const Element*> view) {
	if (view->isHiddenByGroup()) {
		if (const auto group = _data->groups().find(view->data())) {
			if (const auto leader = _viewForItem(group->items.front())) {
				if (!leader->isHiddenByGroup()) {
					_repaintView(leader);
					return;
				}
			}
		}
	}
	_repaintView(view);
}

void ElementHighlighter::updateMessage() {
	if (const auto item = _data->message(_highlightedMessageId)) {
		if (const auto view = _viewForItem(item)) {
			repaintHighlightedItem(view);
			const auto duration = st::activeFadeInDuration
				+ st::activeFadeOutDuration;
			if (crl::now() - _highlightStart <= duration) {
				return;
			}
		}
	}
	_animation.cancel();
	_highlightedMessageId = FullMsgId();
	checkNextHighlight();
}

void ElementHighlighter::clear() {
	_highlightedMessageId = FullMsgId();
	updateMessage();
}

ElementHighlighter::AnimationManager::AnimationManager(
	ElementHighlighter &parent)
: _parent(parent) {
}

bool ElementHighlighter::AnimationManager::animating() const {
	return _simple.animating();
}

float64 ElementHighlighter::AnimationManager::progress() const {
	return _simple.value(0.);
}

void ElementHighlighter::AnimationManager::start() {
	_simple.stop();
	_simple.start(
		[=] { _parent.updateMessage(); },
		0.,
		1.,
		st::activeFadeInDuration + st::activeFadeOutDuration);
}

void ElementHighlighter::AnimationManager::cancel() {
	_simple.stop();
}

} // namespace HistoryView