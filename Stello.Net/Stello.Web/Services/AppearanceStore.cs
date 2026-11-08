using System.Text.Json;
using System.Text.Json.Serialization;
using Microsoft.JSInterop;
using Stello.App.Models;

namespace Stello.Web.Services;

/// <summary>The existing local-storage entry used to initialize the shared appearance.</summary>
public sealed class AppearanceStore(IJSInProcessRuntime js)
{
    private const string Key = "stello.appearance";

    public Appearance Load()
    {
        try
        {
            string? json = js.Invoke<string?>("localStorage.getItem", Key);
            return json is null ? Appearance.Default : JsonSerializer.Deserialize(json, AppearanceJson.Default.Appearance) ?? Appearance.Default;
        }
        catch (Exception exception) when (exception is JsonException or NotSupportedException or JSException)
        {
            return Appearance.Default;
        }
    }

    public void Save(Appearance appearance)
    {
        try
        {
            js.InvokeVoid("localStorage.setItem", Key, JsonSerializer.Serialize(appearance, AppearanceJson.Default.Appearance));
        }
        catch (JSException)
        {
            // The look still changes; it is only not remembered.
        }
    }
}

[JsonSourceGenerationOptions(UseStringEnumConverter = true)]
[JsonSerializable(typeof(Appearance))]
internal sealed partial class AppearanceJson : JsonSerializerContext;
