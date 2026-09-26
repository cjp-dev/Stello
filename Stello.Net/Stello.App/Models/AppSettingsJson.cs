using System.Text.Json.Serialization;

namespace Stello.App.Models;

/// <summary>Settings as readable JSON with enum names; source-generated so it also works trimmed in the browser.</summary>
[JsonSourceGenerationOptions(WriteIndented = true, UseStringEnumConverter = true)]
[JsonSerializable(typeof(AppSettings))]
public sealed partial class AppSettingsJson : JsonSerializerContext;
