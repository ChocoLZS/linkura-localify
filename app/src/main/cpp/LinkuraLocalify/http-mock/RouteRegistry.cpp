#include "RouteRegistry.hpp"

#include "../HookMain.h"
#include "backend/HttpMockBackend.hpp"
#include "offline_api_mock_builtin.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace LinkuraLocal::HttpMock {
    namespace {
        using RouteTable = std::unordered_map<std::string, RegisteredRouteHandler>;

        static void RegisterStaticJson(RouteTable& routes,
                                       std::string_view path,
                                       std::string_view jsonBody,
                                       std::string_view headersText = OfflineApiMockBuiltIn::DefaultHeadersView) {
            routes.emplace(std::string(path), [jsonBody, headersText](const MockRequestContext&, HttpMockBackend&) -> std::optional<MockResponse> {
                return MockResponse{
                    std::string(jsonBody),
                    std::string(headersText),
                    200,
                    "OK (offline mock)",
                };
            });
        }

        static void RegisterBackend(RouteTable& routes,
                                    std::string_view path,
                                    RegisteredRouteHandler handler) {
            routes.emplace(std::string(path), std::move(handler));
        }

        static std::optional<MockResponse> HandleUserLogin(const MockRequestContext& request,
                                                             HttpMockBackend&) {
            const auto playerId = HttpMockBackend::ExtractPayloadStringField(request.payloadJson, "player_id");
            const auto& jsonBody = playerId.empty()
                ? OfflineApiMockBuiltIn::UserLoginNewJsonView
                : OfflineApiMockBuiltIn::UserLoginJsonView;
            return MockResponse{
                std::string(jsonBody),
                std::string(OfflineApiMockBuiltIn::UserLoginHeadersView),
                200,
                "OK (offline mock)",
            };
        }

        static std::optional<MockResponse> HandleArchiveDetail(const MockRequestContext& request,
                                                               HttpMockBackend& backend) {
            const auto fallbackArchiveId = HookShare::Shareable::currentArchiveId;
            auto record = backend.LookupArchiveDetailFromPayload(request.payloadJson, fallbackArchiveId);
            if (!record.has_value()) {
                const auto attemptedKey = HttpMockBackend::ExtractPayloadStringField(request.payloadJson, "archives_id");
                Log::WarnFmt("[HttpMockRouteRegistry] archive_detail not found path=%.*s archives_id=%s fallback=%s backend=%s",
                             static_cast<int>(request.path.size()),
                             request.path.data(),
                             attemptedKey.c_str(),
                             fallbackArchiveId.c_str(),
                             backend.GetStatusSummary().c_str());
                return std::nullopt;
            }

            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }

            return MockResponse{
                std::move(record->body),
                std::move(record->headersText),
                record->statusCode,
                std::move(record->statusDescription),
            };
        }

        static std::optional<MockResponse> HandleCharacterInfo(const MockRequestContext& request,
                                                               HttpMockBackend& backend) {
            auto record = backend.LookupCharacterInfoFromPayload(request.payloadJson);
            if (!record.has_value()) {
                const auto attemptedKey = HttpMockBackend::ExtractPayloadIntegerFieldAsString(request.payloadJson, "character_id");
                Log::WarnFmt("[HttpMockRouteRegistry] character_info not found path=%.*s character_id=%s backend=%s",
                             static_cast<int>(request.path.size()),
                             request.path.data(),
                             attemptedKey.c_str(),
                             backend.GetStatusSummary().c_str());
                return std::nullopt;
            }

            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }

            return MockResponse{
                std::move(record->body),
                std::move(record->headersText),
                record->statusCode,
                std::move(record->statusDescription),
            };
        }

        static std::optional<MockResponse> HandleCardDetail(const MockRequestContext& request,
                                                             HttpMockBackend& backend) {
            auto record = backend.LookupCardDetailFromPayload(request.payloadJson);
            if (!record.has_value()) {
                const auto attemptedKey = HttpMockBackend::ExtractPayloadStringField(request.payloadJson, "d_card_datas_id");
                Log::WarnFmt("[HttpMockRouteRegistry] card_detail not found path=%.*s d_card_datas_id=%s backend=%s",
                             static_cast<int>(request.path.size()),
                             request.path.data(),
                             attemptedKey.c_str(),
                             backend.GetStatusSummary().c_str());
                return std::nullopt;
            }

            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }

            return MockResponse{
                std::move(record->body),
                std::move(record->headersText),
                record->statusCode,
                std::move(record->statusDescription),
            };
        }

        static std::optional<MockResponse> HandleCheckStyleLevelUp(const MockRequestContext& request,
                                                                    HttpMockBackend& backend) {
            auto record = backend.CheckStyleLevelUp(request.payloadJson);
            if (!record.has_value()) {
                return std::nullopt;
            }
            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }
            return MockResponse{
                std::move(record->body), std::move(record->headersText),
                record->statusCode, std::move(record->statusDescription),
            };
        }

        static std::optional<MockResponse> HandleItemDetail(const MockRequestContext& request,
                                                             HttpMockBackend& backend) {
            auto record = backend.LookupItemDetailFromPayload(request.payloadJson);
            if (!record.has_value()) {
                const auto attemptedKey = HttpMockBackend::ExtractPayloadStringField(request.payloadJson, "d_item_datas_id");
                Log::WarnFmt("[HttpMockRouteRegistry] item_detail not found path=%.*s d_item_datas_id=%s backend=%s",
                             static_cast<int>(request.path.size()),
                             request.path.data(),
                             attemptedKey.c_str(),
                             backend.GetStatusSummary().c_str());
                return std::nullopt;
            }

            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }

            return MockResponse{
                std::move(record->body),
                std::move(record->headersText),
                record->statusCode,
                std::move(record->statusDescription),
            };
        }

        static std::optional<MockResponse> HandleDeckGetList(const MockRequestContext&,
                                                                HttpMockBackend& backend) {
            auto record = backend.GetDeckListResponse();
            if (!record.has_value()) {
                Log::WarnFmt("[HttpMockRouteRegistry] deck get_list failed backend=%s",
                             backend.GetStatusSummary().c_str());
                return std::nullopt;
            }

            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }

            return MockResponse{
                std::move(record->body),
                std::move(record->headersText),
                record->statusCode,
                std::move(record->statusDescription),
            };
        }

        static std::optional<MockResponse> HandleDeckModifyDeckList(const MockRequestContext& request,
                                                                     HttpMockBackend& backend) {
            auto record = backend.ModifyDeckList(request.payloadJson);
            if (!record.has_value()) {
                Log::WarnFmt("[HttpMockRouteRegistry] deck modify_deck_list failed backend=%s",
                             backend.GetStatusSummary().c_str());
                return std::nullopt;
            }

            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }

            return MockResponse{
                std::move(record->body),
                std::move(record->headersText),
                record->statusCode,
                std::move(record->statusDescription),
            };
        }

        static std::optional<MockResponse> HandleRhythmGameHome(const MockRequestContext&,
                                                                  HttpMockBackend& backend) {
            auto record = backend.GetRhythmGameHome();
            if (!record.has_value()) {
                Log::WarnFmt("[HttpMockRouteRegistry] rhythm_game/home failed backend=%s",
                             backend.GetStatusSummary().c_str());
                return std::nullopt;
            }
            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }
            return MockResponse{
                std::move(record->body), std::move(record->headersText),
                record->statusCode, std::move(record->statusDescription),
            };
        }

        static std::optional<MockResponse> HandleRhythmGameSetStart(const MockRequestContext& request,
                                                                      HttpMockBackend& backend) {
            auto record = backend.RhythmGameSetStart(request.payloadJson);
            if (!record.has_value()) {
                return std::nullopt;
            }
            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }
            return MockResponse{
                std::move(record->body), std::move(record->headersText),
                record->statusCode, std::move(record->statusDescription),
            };
        }

        static std::optional<MockResponse> HandleRhythmGameSetFinish(const MockRequestContext& request,
                                                                      HttpMockBackend& backend) {
            auto record = backend.RhythmGameSetFinish(request.payloadJson);
            if (!record.has_value()) {
                Log::WarnFmt("[HttpMockRouteRegistry] rhythm_game_live/set_finish failed backend=%s",
                             backend.GetStatusSummary().c_str());
                return std::nullopt;
            }
            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }
            return MockResponse{
                std::move(record->body), std::move(record->headersText),
                record->statusCode, std::move(record->statusDescription),
            };
        }

        static std::optional<MockResponse> HandleRhythmGameDeckModify(const MockRequestContext& request,
                                                                       HttpMockBackend& backend) {
            auto record = backend.ModifyRhythmGameDeckList(request.payloadJson);
            if (!record.has_value()) {
                Log::WarnFmt("[HttpMockRouteRegistry] rhythm_game_deck/modify failed backend=%s",
                             backend.GetStatusSummary().c_str());
                return std::nullopt;
            }
            if (record->headersText.empty()) {
                record->headersText = std::string(OfflineApiMockBuiltIn::DefaultHeadersView);
            }
            return MockResponse{
                std::move(record->body), std::move(record->headersText),
                record->statusCode, std::move(record->statusDescription),
            };
        }

        static RouteTable BuildRoutes() {
            RouteTable routes;

            RegisterStaticJson(routes, "/v1/account/connect", OfflineApiMockBuiltIn::AccountConnectJsonView);
            RegisterStaticJson(routes, "/v1/user/push/devices", "null");
            RegisterBackend(routes, "/v1/user/login", HandleUserLogin);
            RegisterStaticJson(routes, "/v1/home/get_home", OfflineApiMockBuiltIn::HomeGetHomeJsonView);
            RegisterStaticJson(routes, "/v1/home/get_custom_setting", OfflineApiMockBuiltIn::GetCustomSettingJsonView);
            RegisterStaticJson(routes, "/v1/webview/school_idol_connect_post/get_theme_list", OfflineApiMockBuiltIn::WebviewSchoolIdolConnectPostGetThemeListJsonView);
            RegisterStaticJson(routes, "/v1/follow/live_chat_group_list", OfflineApiMockBuiltIn::FollowLiveChatGroupListJsonView);
            RegisterStaticJson(routes, "/v1/archive/get_home", OfflineApiMockBuiltIn::ArchiveGetHomeJsonView);
            RegisterStaticJson(routes, "/v1/archive/get_archive_list", OfflineApiMockBuiltIn::ArchiveGetArchiveListJsonView);
            RegisterStaticJson(routes, "/v1/out_quest_live/get_quest_top", OfflineApiMockBuiltIn::OutQuestLiveGetQuestTopJsonView);
            RegisterStaticJson(routes, "/v1/out_quest_live/daily/get_stage_select", OfflineApiMockBuiltIn::OutQuestLiveDailyGetStageSelectJsonView);
            RegisterStaticJson(routes, "/v1/user/card/get_list", OfflineApiMockBuiltIn::UserCardGetListJsonView);
            RegisterBackend(routes, "/v1/user/card/get_detail", HandleCardDetail);
            RegisterBackend(routes, "/v1/user/card/check_style_level_up", HandleCheckStyleLevelUp);
            RegisterStaticJson(routes, "/v1/user/items/get_list", OfflineApiMockBuiltIn::UserItemsGetListJsonView);
            RegisterBackend(routes, "/v1/user/item/get_detail", HandleItemDetail);
            RegisterBackend(routes, "/v1/collection/get_character_info", HandleCharacterInfo);
            RegisterStaticJson(routes, "/v1/activity_record/get_top", OfflineApiMockBuiltIn::ActivityRecordGetTopJsonView);
            RegisterStaticJson(routes, "/v1/activity_record/play_adv_data", OfflineApiMockBuiltIn::ActivityRecordPlayAdvDataJsonView);

            RegisterBackend(routes, "/v1/archive/get_with_archive_data", HandleArchiveDetail);
            RegisterBackend(routes, "/v1/archive/get_fes_archive_data", HandleArchiveDetail);

            RegisterStaticJson(routes, "/v1/profile/get_mute_list", OfflineApiMockBuiltIn::ProfileGetMuteListJsonView);

            RegisterBackend(routes, "/v1/user/deck/get_list", HandleDeckGetList);
            RegisterBackend(routes, "/v1/user/deck/modify_deck_list", HandleDeckModifyDeckList);
            RegisterStaticJson(routes, "/v1/user/deck/notify_auto_deck", "null");
            RegisterStaticJson(routes, "/v1/collection/get_music_list", OfflineApiMockBuiltIn::CollectionGetMusicListJsonView);
            RegisterStaticJson(routes, "/v1/collection/get_sticker_list", OfflineApiMockBuiltIn::CollectionGetStickerListJsonView);
            RegisterStaticJson(routes, "/v1/collection/get_gallary_list", OfflineApiMockBuiltIn::CollectionGetGallaryListJsonView);

            RegisterStaticJson(routes, "/v1/step_up_beginner_mission/get_list", OfflineApiMockBuiltIn::StepUpBeginnerMissionGetListJsonView);

            RegisterBackend(routes, "/v1/rhythm_game/home", HandleRhythmGameHome);
            RegisterBackend(routes, "/v1/rhythm_game_live/set_start", HandleRhythmGameSetStart);
            RegisterBackend(routes, "/v1/rhythm_game_live/set_finish", HandleRhythmGameSetFinish);
            RegisterBackend(routes, "/v1/rhythm_game_deck/modify_deck_list", HandleRhythmGameDeckModify);
            RegisterStaticJson(routes, "/v1/rhythm_game/receive_class_mission", "null");

            return routes;
        }

        static const RouteTable& GetRoutes() {
            static const RouteTable routes = BuildRoutes();
            return routes;
        }
    }

    std::optional<MockResponse> ResolveRegisteredRoute(const MockRequestContext& request) {
        const auto& routes = GetRoutes();
        const auto it = routes.find(std::string(request.path));
        if (it == routes.end()) {
            return std::nullopt;
        }
        return it->second(request, HttpMockBackend::Get());
    }
}
