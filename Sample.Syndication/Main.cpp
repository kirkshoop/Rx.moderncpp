#include "pch.h"

using namespace std;
using namespace Modern;

using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Web::Syndication;

Rx::observable<SyndicationItem> Sample(Rx::observable<Uri>& feeds)
{
    SyndicationClient const client;

    return feeds.map(
        [=](Uri uri) {
        return Rx::start_async([=]() { return client.RetrieveFeedAsync(uri); })
            .map([](SyndicationFeed const & feed) {
                return Rx::iterate(Rx::to_vector(feed.Items()));
            })
            .merge();
        })
        .concat();
}

Rx::observable<String> SampleWithProgress(Rx::observable<Uri>& feeds)
{
    SyndicationClient const client;

    return feeds.map(
        [=](Uri uri) {
            return Rx::start_async_with_progress([=]() { return client.RetrieveFeedAsync(uri); })
                .map(Rx::apply_to(
                    [=](Rx::observable<SyndicationFeed> result, Rx::observable<RetrievalProgress> progress) {
                        auto start = chrono::system_clock::now();
                        return Rx::from(
                            Rx::from(String(L"\nFrom: "), uri.DisplayUri(), String(L"\n\n"))
                                .as_dynamic(),
                            progress
                                .map([=](RetrievalProgress const & rp) {
                                    auto now = chrono::system_clock::now();
                                    wstringstream ss;
                                    fill_n(ostream_iterator<wchar_t, wchar_t>(ss), 40, L'\b');
                                    ss << L"Downloaded: " << rp.BytesRetrieved / 1024.0 << L"K in " << chrono::duration_cast<chrono::milliseconds>(now - start).count() << L"ms        ";
                                    return String(ss.str().c_str());
                                })
                                .as_dynamic(),
                            Rx::from(String(L"\n\n"))
                                .as_dynamic(),
                            result
                                .map([](SyndicationFeed const & feed) {
                                    return Rx::iterate(Rx::to_vector(feed.Items()))
                                        .map([](SyndicationItem const & item) {
                                            std::wstring title = item.Title().Text().Buffer();
                                            title += L"\n";
                                            return String(title.c_str());
                                        });
                                    })
                                    .merge()
                                    .as_dynamic())
                            .concat();
                    }))
                .retry(3)
                .merge();
        })
        .concat();
}

int main()
{
    Initialize();

    auto feeds = Rx::from(
        Uri(L"http://feeds.bbci.co.uk/news/world/rss.xml"),
        Uri(L"http://earthquake.usgs.gov/earthquakes/feed/v1.0/summary/all_week.atom"),
        Uri(L"http://rss.cnn.com/rss/cnn_topstories.rss"),
        Uri(L"http://kennykerr.ca/feed"))
        .skip(3)
        .as_dynamic();

    printf("async\n\n");

    Sample(feeds)
        .as_blocking()
        .subscribe([](SyndicationItem const & item) {
            String title = item.Title().Text();

            printf("%ls\n", title.Buffer());
        }, &Rx::print_error);

    printf("\nwith progress\n\n");

    auto repeateverysecond = Rx::interval(chrono::steady_clock::now(), chrono::seconds(1))
        .map([=](long) {return feeds; })
        .take(4)
        .merge()
        .as_dynamic();

    SampleWithProgress(repeateverysecond)
        .as_blocking()
        .subscribe([](String const & output) {
            printf("%ls", output.Buffer());
        }, &Rx::print_error);
}

